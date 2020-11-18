/////////////////////////////////////////////////////////
/////////MucitPilot 2020 Wifi LedSaat - Wifi Bağlanma Otomasyonlu///////////////////
//Wifi üzerinden hassas saat ve tarihi alıyor ancak kendi saatini kullanıyor. Sunucuya belli aralıklarla bağlanıp
//saati ve tarihi güncelliyor. Bu sayede sürekli sunucuya bağlanmak zorunda kalmıyor.
//2 adet alarm mevcut ve Alarm ile ilgili ayarlar kartın IP adresine bağlanarak web sunucu sayfası üzerinden değiştirilebiliyor.
//WifiManager kütüphanesi ile ağ bilgilerini koda yazmaya gerek kalmadan kullanıcıdan alabiliyor ve kullanıcı değiştirmek isterse,
//alarm reset butonuna 3snden fazla basarak resetleyip tekrar ağ bilgileri giriş sayfası açılabiliyor.///////////////////
/////////////////////////////////////////////////////////

//Gerekli Kütüphaneleri Yüklüyoruz////////////
#include <NTPtimeESP.h>//NTP sunucusundan zamanı güncellemek için https://github.com/SensorsIot/NTPtimeESP
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <DFPlayerMini_Fast.h> //MP3player kütüphanesi https://github.com/PowerBroker2/DFPlayerMini_Fast
#include "Fonts_data.h"
#include <SoftwareSerial.h>//mp3player ile software serial üzerinden haberleşeceğiz
#include <ESP8266WebServer.h>       // Web sunucusu kurarak alarm güncellemesini wifi üzerinden yapacağız
#include <ESP8266mDNS.h>            // Web sunucusu için
#include <EEPROM.h> //alarm bilgilerini eproma yazıp okuyacağız.
#include "ESP8266TimerInterrupt.h" //interrupt kullanımı için gerekli kütüphaneyi yarattık.https://github.com/khoih-prog/ESP8266TimerInterrupt

////////////WIFI MANAGER KÜTÜPHANESİNİ KULLANACAĞIZ//////////////////////////
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager


//////zaman kesmesi için bir hata kontrol değişkeni tanımladık
#define TIMER_INTERRUPT_DEBUG      1

//software serial kullanarak MP3Player ile haberleşiyoruz
SoftwareSerial mySerial(D4, D3); // RXpin, TXpin (Mp3player TX pini ESP rx pinine)

//web sunucusu için gerekli nesneleri ve değişkenleri oluşturuyoruz.
WiFiClient espClient;
MDNSResponder mdns;
ESP8266WebServer server(80);
String webPage = "";
String webStat = "";
String webFooter = "";
String temp = "";
const int msglen = 500;


//LED modül tanımlamaları ve PIN tanımlamaları
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  //kullanılan modül tipi. 4ü bir arada modeller için FC16_HW kullanın
#define MAX_DEVICES 4 //kaç modül bağlı olduğu
#define CLK_PIN   D5 //pinler
#define DATA_PIN  D7
#define CS_PIN    D8
byte intensity = 1;                // Led matris parlaklığı 0-15 arası, 0 en düşük

//mp3 player nesnesi oluşturduk
DFPlayerMini_Fast myMP3;

// NTP sunucu adresini verdik ve bir TarihZaman nesnesi oluşturduk
NTPtime NTPch("pool.ntp.org");
strDateTime dateTime;

// Bir adet Parola nesnesi yaratıyoruz
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Kayan Yazı Paramatreleri
uint8_t scrollSpeed = 80;    // Kayma hızı, rakam küçüldükçe hız artar
//Giriş ve çıkışta kayma efektlerini seçin
textEffect_t scrollEffectin = PA_SCROLL_LEFT; //PA_SCROLL_LEFT,PA_SCROLL_RIGHT,PA_SCROLL_UP,PA_SCROLL_DOWN
textEffect_t scrollEffectout = PA_SCROLL_LEFT; //PA_SCROLL_LEFT,PA_SCROLL_RIGHT,PA_SCROLL_UP,PA_SCROLL_DOWN
//metin ne tarafa hizalanacak
textPosition_t scrollAlign = PA_CENTER;//PA_CENTER,PA_LEFT,PA_RIGHT
uint16_t scrollPause = 0; // Metin kaç milisaniye sabit olarak gösterimde kalacak
uint8_t  inFX, outFX; //giriş ve çıkışta uygulanacak efektler
textEffect_t  effect[] = //efektleri tuttuğumuz dizi
{

  //kütüphanede tanımlı efektler...
  PA_PRINT, //0
  PA_SCAN_HORIZ,//1
  PA_SCROLL_LEFT,//2
  PA_WIPE, //3
  PA_SCROLL_UP_LEFT,//4
  PA_SCROLL_UP,//5
  PA_OPENING_CURSOR,//6
  PA_GROW_UP,//7
  PA_MESH,//8
  PA_SCROLL_UP_RIGHT,//9
  PA_BLINDS,//10
  PA_CLOSING,//11
  PA_RANDOM,//12
  PA_GROW_DOWN,//13
  PA_SCAN_VERT,//14
  PA_SCROLL_DOWN_LEFT,//15
  PA_WIPE_CURSOR,//16
  PA_DISSOLVE,//17
  PA_OPENING,//18
  PA_CLOSING_CURSOR, //19
  PA_SCROLL_DOWN_RIGHT,//20
  PA_SCROLL_RIGHT,//21
  PA_SLICE,//22
  PA_SCROLL_DOWN,//23
  PA_NO_EFFECT,//24
};


#define  BUF_SIZE  75 //genel olarak kullanacağımız metin uzunluğu
char mesajyaz[BUF_SIZE]; //metinleri mesajyaz değişkeni ile led ekrana yazdıracağız. Bu değişken sabittir. Ekranda yazılacak
//bilgi değiştirilmek istendiğinde kodun içinde mesajyaz değişkeni güncellenir.
const uint16_t WAIT_TIME = 0; //ilk açılış mesajı için yazının ekranda sabit bekleme süresi
String mesaj = "Sunucuya Baglaniliyor..."; //ilk açılış mesajı (Gerçi ben aldığımız IP adresini Yazdırıyorum)

//Gerekli Değişkenlerimizi tanımladık
String tamsaat = "";
String tarih = "";
String alarm = "ALARM";
String alarm2 = "ALARM2";
String gun_gecici;
static bool setalarm = false;  // alarm devrede mi?
static bool alarmbitti = false;
byte actualHour, actualMinute;
byte alarmHour, alarmMinute;
byte alarmHour2, alarmMinute2;
byte actualsecond = 0;
int actualyear;
byte actualMonth;
byte actualday;
String trgun;
String serversaati = "";
int alarmduration = 15; //saniye olarak alarm kaç saniye sürecek
boolean dakikadegisti = false;
boolean saniyedegisti = false;
int sayac = 0;
int senkron = 1; //saat kodumuz NTP sunucusundan kaç dakikada bir güncelleme alsın (1-59 arası girilmeli)
int alarmdurum = 0; //alarm açık kapalı durumu
int alarmdurum2 = 0; //alarm2 açık kapalı durumu
int buttonSayac = 0;
boolean button = false;
bool res; //wifimanager oto bağlantı durumu takibi için
bool resetle = false; //saatten sonra ekrana reset yazdırmak için takip
bool resetle2 = false;//saatten sonra ekrana reset yazdırmak için takip
bool resetecek=false;//saatten sonra ekrana reset yazdırmak için takip


//////////Bir adet wifimanager nesnesi oluşturduk.////////////////////////////////
WiFiManager wm;



////////////INTERRUPT kullanarak saniyeyi hassas takip edeceğiz//////////////////
//////millis ile yapmaya çalışırsak kodun durduğu yerlerde kıyaslama yapamayacağımız için
//millis işe yaramaz ve zaman kaymaları atlamaları oluyor. Onun yerine arka planda her bir saniyeyi takip eden bir zaman kesmesi
//kullanacağız.
volatile uint32_t lastMillis = 0;

void ICACHE_RAM_ATTR TimerHandler(void)
{


#if (TIMER_INTERRUPT_DEBUG > 0)
  //  Serial.println("Delta ms = " + String(millis() - lastMillis));
  lastMillis = millis();
#endif

  saatguncelle();//saat güncelleme fonksiyonu ile her saniyede bir atacak bir saat dakika saniye sayacı çalıştırıyoruz

  //alarm ile ilgili kontrolü kesme içinde yapıyoruz ki o esnada saat ekranda olmasa bile arka planda alarm konrolü hassas çalışabilsin.
  if (setalarm) { //eğer alarm devredeyse
    sayac++; //sayac değişkeni ile alarmduration süresi kadar çalmasına izin verip

    if (sayac == alarmduration) { //süre dolduysa alarmı durduruyoruz
      alarmdurdur();
      sayac = 0;

    }
  }
}

//Zaman Kesmesi için belirlediğimi aralık doğal olarak 1sn
#define TIMER_INTERVAL_MS        1000

// Zaman kesmesini kullanmak için bir nesne yarattık.
ESP8266Timer ITimer;




/////////////SETUP BÖLÜMÜ-1 DEFA ÇALIŞIR/////////////////////////////
void setup() {
  pinMode(D1, INPUT_PULLUP);

  Serial.begin(115200);
  EEPROM.begin(512);
  mySerial.begin(9600);
  myMP3.begin(mySerial);
  myMP3.stop(); //mp3 playerı koda girmeden önce her ihtimale karşı susturuyoruz.


//////////////WIFI MANAGER AYAR BLOĞU//////////////////////////////////////////////////

  //wm.resetSettings(); //bu satırı açarsak hafızadaki wifi ayarlarını temizler.
  WiFi.mode(WIFI_STA); // Özellikle modu station'a ayarlıyoruz.

  res = wm.autoConnect("LedSaat", "12345678"); // Wifimanager bağlanma satırı. Ağ adı olarak görünmesini istediğimiz
  // ismi ve belirleyeceğimiz şifreyi tanımladık. İstersek şifre de girmeyebiliriz. Korumasız ağ olarak görünür.
  
  if (!res) {
    Serial.println("Bağlantı Sağlanamadı");
    // ESP.restart();
  }
  else {
    //buraya gelmişse WiFi'ya bağlanmış demektir.
    Serial.println("Ağ Bağlantısı Kuruldu");
  }


/////////////////////////////////////////////////////////////////////////////////////////



  P.begin();//parola kayan yazı nesnesini başlatıyoruz

  Serial.println(WiFi.localIP());

  mesaj = WiFi.localIP().toString();//aldığımız IP adresini ilk açılış mesajına yazdıralım ki hangi IP'yi almışız görelim

  //web sunucu ile ilgili fonksiyonlar.
  server.on("/", handleMainPage); //aynı sayfaya dönecek
  server.on("/set", handleSetCommand);        // parametreleri koda set ön eki ile göndereceğiz.
  server.begin(); //sunucuyu başlattık

  //EPROM'dan alarm ile ilgili bilgileri okuyoruz.
  alarmduration = EEPROM.read(101);  //alarm kaç saniye çalacak
  delay(100);

  alarmdurum = EEPROM.read(102); //alarm açık mı kapalı mı
  delay(100);

  alarmHour = EEPROM.read(103); //alarm saati
  delay(100);
  if (alarmHour == 255) {
    alarmHour = 0;
  }

  alarmMinute = EEPROM.read(104); //alarm dakikası
  delay(100);
  if (alarmMinute == 255) {
    alarmMinute = 0;
  }

  alarmHour2 = EEPROM.read(105); //alarm saati 2
  delay(100);
  if (alarmHour2 == 255) {
    alarmHour2 = 0;
  }

  alarmMinute2 = EEPROM.read(106); //alarm dakikası 2
  delay(100);
  if (alarmMinute2 == 255) {
    alarmMinute2 = 0;
  }

  alarmdurum2 = EEPROM.read(107); //alarm açık mı kapalı mı 2
  delay(100);

  //mesajların LED ekranda gösterilmesi için gerekli ilk tanımlamayı yaptık
  P.displayText(mesajyaz, scrollAlign, scrollSpeed, scrollPause, scrollEffectin, scrollEffectout); //inFX, outFX
  inFX = 0; //açılış mesajının giriş çıkış efektlerini seçtik
  outFX = 0;
  P.setIntensity(intensity);//açılış mesajının parlaklık seviyesini seçtik (0-15 arası)

  mesaj.toCharArray(mesajyaz, BUF_SIZE); //toCharArray komutu ile mesajı yazdırılabilecek tek karakterlerden oluşan char dizisine çevirdik //ÖNEMLİ
  P.setFont(minikFont);//açılış mesajı için font tipi
  //  myMP3.stop(); //mp3 playerı koda girmeden önce her ihtimale karşı susturuyoruz.


  // Oluşturduğumuz zaman kesmesini başlatıyoruz.
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler)) //her saniyede bir yukarıda yarattığımız TimerHandler fonksiyonunu çalıştıracak.
  {
    lastMillis = millis();
    Serial.println("ITimer Zaman Kesmesi Başlatılıyor, millis() = " + String(lastMillis));
  }
  else
    Serial.println("Kesme düzgün ayarlanamıyor. Başka bir frekans veya aralık seçin");

  /////////////////////////////////////////////
  //ilk girişte sunucudan verileri düzgün çekmesi için döngü içinde ilk verileri almasını sağladık
  while (!dateTime.valid) {
    dateTime = NTPch.getNTPtime(3.0, 0);
  }
  //Sunucudan gelen verileri saatsenkron fonksiyonu ile bizim kesme ile yaptığımız manuel sayan saati güncellemek için kullandık.
  saatsenkron();

  ////D1 dijital pinine tanımladığımız donanımsal interrupt için gerekli tanımlama
  attachInterrupt(digitalPinToInterrupt(D1), Alarmstop, CHANGE); //pinin durumu CHANGE, RISING ve FALLING olabilir.


}
////////////////////////SETUP SONU/////////////////////////////////////////////






/////////////////LOOP KISMI- SÜREKLİ DÖNER///////////////////////////////////
void loop()
{

  if (P.displayAnimate()) //LED Matrix animasyonlarımız bu satır ve displayReset komutu arasında oynar
  {
  
    //eğer butona 3snden fazla basılarak wifi ayarları resetleme işlemi yapılmışsa;
    if (resetle) { //resetle değişkeni butona 3snden fazla basılınca TRUE yaptığımız bir kontrol değişkeni
      resetle = false;
      resetle2 = true; //resetle2 ise resetleye girdikten sonra aşağıdaki alarm durdurma kısmına girmesi için tanımladığmız bir değişken
      P.displayClear();
      P.setSpeed(70);
      String rst = "RESET"; //ekrana reset yazdıralım
      rst.toCharArray(mesajyaz, BUF_SIZE); //oluşturduğumuz string metni Char dizisine çevirmeliyiz
      inFX = 24; //giriş efekti
      outFX = 24; //çıkış efekti
      P.setTextEffect(effect[inFX], effect[outFX]);//efektleri uygula
      P.setPause(0); //metin ekrranda sabit kaç milisaniye kalacak
      P.displayReset();
    } 
    else if (resetle2) {
      resetle2 = false;

      alarmdurdur(); //mp3 playerı koda girmeden önce her ihtimale karşı susturuyoruz.

      WiFi.disconnect(true);//ESPnin bağlantısını ağdan koparıyoruz
      delay(2000);
      wm.resetSettings();//hafızadaki ağ ayarlarını temizliyoruz
      delay(3000);
      ESP.reset();//esp'ye reset atıp yeniden başlatıyoruz
      delay(5000);


    }

    P.setIntensity(1); //1-15 arası 15 en yüksek //yüksek olduğunda USBden fazla akım çekebilir.
    P.setFont(daruzunsayi);//kullanılacak yazı tipi seçildi.
    P.setPause(0);//mesaj ekranda bekletilmeyecek

    if (dakikadegisti) { //saydırdığımız saat içinde eğer yeni bir dakikaya geçilmiş ise
      //saat değişti değişkeni true olacağı için bu sayede dakikada bir yapmak istediğimi
      //alarmı kontrol etme ve saat bilgilerini sunucudan güncelleme gibi işlemleri kontrol edebiliyoruz.

      alarmkontrol(); //alarmın devreye girmesi gerekiyor mu bak

      senkronkontrol(senkron);//sıfır olamaz. Kaç dakikada bir update yapılacağını belirliyor.

      dakikadegisti = false; //kontrolümüzü yaptık ve yeni bir dakika için değişkeni false a çektik.
    }

    tarihgun();//bu fonksiyon ile tarih ve gün bilgisini birleştirip bir stringe çevirdik.

    //switch case kullanarak 10. ve 40. saniyelerde ekrana tarih-gün bilgisinin yazılmasını sağlyoruz.
    switch (actualsecond) {
      case 10:
      case 40:
        P.displayClear();
        P.setSpeed(70);
        trgun.toCharArray(mesajyaz, BUF_SIZE); //oluşturduğumuz string metni Char dizisine çevirmeliyiz
        inFX = 2; //giriş efekti sola kay
        outFX = 2; //çıkış efekti solakay
        P.setTextEffect(effect[inFX], effect[outFX]);//efektleri uygula
        P.setPause(0); //metin ekrranda sabit kaç milisaniye kalacak
        //P.displayReset();
        break;

      default:
        break;

    }

    if (saniyedegisti) { //saniye değişti değişkeni ile her yeni bir saniyede yapılacakları kontrol ediyoruz.
      
      if (!resetecek){ //resetleme işlemi yapılMAmışsa;
        saatyazdir();//her yeni saniyede bir saati ekrana yazdırıyoruz.
        saniyedegisti = false; //işimiz bitince değişkeni update ediyoruz.
      } 
      else { //eğer butona 3snden fazla basılarak reset yapılmışsa
      resetecek=false;  
      P.displayClear();
      P.setSpeed(70);
      String rst = "RESET";
      rst.toCharArray(mesajyaz, BUF_SIZE); //oluşturduğumuz string metni Char dizisine çevirmeliyiz
      inFX = 24; //giriş efekti
      outFX = 24; //çıkış efekti
      P.setTextEffect(effect[inFX], effect[outFX]);//efektleri uygula
      P.setPause(0); //metin ekrranda sabit kaç milisaniye kalacak
      P.displayReset();
      }
      
    }

    
    if (setalarm) { //alarm devrede ise ekrana alarm yazdırıyoruz
      P.displayClear();
      P.setSpeed(70);
      alarm.toCharArray(mesajyaz, BUF_SIZE); //oluşturduğumuz string metni Char dizisine çevirmeliyiz
      inFX = 6; //giriş efekti
      outFX = 19; //çıkış efekti
      P.setTextEffect(effect[inFX], effect[outFX]);//efektleri uygula
      P.setPause(0); //metin ekrranda sabit kaç milisaniye kalacak
      P.displayReset();

    }

    P.displayReset(); //animasyonlar başa döndürüldü.
  }

  server.handleClient(); //sunucu işlemleri yapılıyor.
}


/////////////GEREKLİ FONKSİYONLARIMIZ//////////////////
//bu fonksiyon ile kendi basit saatimizi yapmış olduk. Her saniye sunucuya bağlanmak yerine belli zamanlarda sunucudan,
//hassas saat bilgisini alıp buradaki saati güncelleyeceğiz. Ama ekranımızda oynayan saat aslında bu olacak.
void saatguncelle() {
  actualsecond++;
  if (actualsecond == 60) {
    actualsecond = 0;
    actualMinute++;
    dakikadegisti = true;
  }
  if (actualMinute == 60) {
    actualMinute = 0;
    actualHour++;
  }
  if (actualHour == 24) {
    actualHour = 0;

  }

  saniyedegisti = true;

}


//Bu fonksiyon ile yularıda oluşturmduğumuz saati String'e çevirip formatını ayarlayıp
//LEDMatrixe yazdırıyoruz.
String saatyazdir() {

  //saat, dakika ve saniye <10 ise başına 0 hanesi ekliyoruz.
  if (actualHour < 10) {
    tamsaat = "0" + String(actualHour);
  }
  else {
    tamsaat = String(actualHour);
  }
  if (actualMinute < 10) {
    tamsaat = tamsaat + ":" + "0" + String(actualMinute);
  }
  else {
    tamsaat = tamsaat + ":" + String(actualMinute);
  }
  if (actualsecond < 10) {
    tamsaat = tamsaat + ":" + "0" + String(actualsecond);
  }
  else {
    tamsaat = tamsaat + ":" + String(actualsecond);
  }

  //LED'e yazdırma kısmı
  tamsaat.toCharArray(mesajyaz, BUF_SIZE); //oluşturduğumuz string metni Char dizisine çevirmeliyiz
  inFX = 24; //giriş efekti
  outFX = 24; //çıkış efekti
  P.setTextEffect(effect[inFX], effect[outFX]);//efektleri uygula
  P.setSpeed(10); // animasyondaki oynama hızını tanımladık

  return tamsaat;
}


//Bu fonksiyon ile NTP sunucusundan veri çekip bu veriler ile sistemdeki değişkenleri güncellemiş oluyoruz.
void saatsenkron() {

  if ( WiFi.status() == WL_CONNECTED ){
  do {  //doğru bilgiyi alana kadar dön
    dateTime = NTPch.getNTPtime(3.0, 0);

  } while (!dateTime.valid);
  

  //doğru bilgi gelmişse değişkenleri güncelle
  if (dateTime.valid) {
    actualHour = dateTime.hour;
    actualMinute = dateTime.minute;
    actualsecond = dateTime.second;
    actualyear = dateTime.year;
    actualMonth = dateTime.month;
    actualday = dateTime.day;
  }
  tarihyazdir();
  gunyazdir();

  //DEBUG için açabilirsiniz.
  //  Serial.print("senkron: ");
  //  Serial.print(actualHour);
  //  Serial.print(":");
  //  Serial.print(actualMinute);
  //  Serial.print(":");
  //  Serial.print(actualsecond);
  //  Serial.println();
  }
}


//tarih bilgisini düzenleyip yazdırıyoruz.
String tarihyazdir() {


  if (actualday < 10) {
    tarih = "0" + String(actualday);
  }
  else {
    tarih = String(actualday);
  }
  if (actualMonth < 10) {
    tarih = tarih + "." + "0" + String(actualMonth);
  }
  else {
    tarih = tarih + "." + String(actualMonth);
  }
  tarih = tarih + "." + String(actualyear);
  Serial.println(tarih);
  return tarih;
}

//Ay bilgisi için kullanılabilir.
void ayyazdir() {

  byte actualMonth = dateTime.month;
  String ay_gecici;
  switch (actualMonth) {
    case 1 :
      ay_gecici = "Ocak";
      break;
    case 2:
      ay_gecici = "Subat";
      break;
    case 3:
      ay_gecici = "Mart";
      break;
    case 4:
      ay_gecici = "Nisan";
      break;
    case 5:
      ay_gecici = "Mayis";
      break;
    case 6:
      ay_gecici = "Haziran";
      break;
    case 7:
      ay_gecici = "Temmuz";
      break;
    case 8:
      ay_gecici = "Agustos";
      break;
    case 9:
      ay_gecici = "Eylul";
      break;
    case 10:
      ay_gecici = "Ekim";
      break;
    case 11:
      ay_gecici = "Kasim";
      break;
    case 12:
      ay_gecici = "Aralik";
      break;
  }
  ay_gecici.toCharArray(mesajyaz, BUF_SIZE); //oluşturduğumuz string metni Char dizisine çevirmeliyiz
  inFX = 24; //giriş efekti
  outFX = 24; //çıkış efekti
  P.setTextEffect(effect[inFX], effect[outFX]);//efektleri uygula
  //P.setPause(1000); //metin ekrranda sabit kaç milisaniye kalacak
  Serial.println(ay_gecici);


  //Gün bilgisini yazdırmak için kullandığımız fonksiyon.
}
String gunyazdir() {

  byte actualdayofWeek = dateTime.dayofWeek; //pazar 1

  switch (actualdayofWeek) {
    case 1 :
      gun_gecici = "PAZAR";
      break;
    case 2:
      gun_gecici = "PAZARTESi";
      break;
    case 3:
      gun_gecici = "SALI";
      break;
    case 4:
      gun_gecici = "CARSAMBA";
      break;
    case 5:
      gun_gecici = "PERSEMBE";
      break;
    case 6:
      gun_gecici = "CUMA";
      break;
    case 7:
      gun_gecici = "CUMARTESi";
      break;
  }


  Serial.println(gun_gecici);
  return gun_gecici;
}


//tarih ve günü arka arkaya yazdırmak için
String tarihgun() {
  trgun = tarih + "      " + gun_gecici;

  return trgun;
}


//kaç dakikada bir sunucudan senkronizasyon yapılacağını kontrol ettiğimiz fonksiyon.
void senkronkontrol(int dakika) {
  if (actualMinute % dakika == 0) {
    saatsenkron();

  }
  Serial.print(" senkron edildi:");
  Serial.println(dakika);
}


//alarmın devreye girip girmeyeceğini kontrol ettiğimiz fonksiyon.
void alarmkontrol() {
  //eğer her iki alarm da aynı saate ayarlı ise sadece alarm 1 i oynat
  if ((alarmHour == alarmHour2) && (alarmMinute == alarmMinute2)) {
    alarmdurum2 == 0;
  }
  if ((alarmHour == actualHour) && (alarmMinute == actualMinute) && alarmdurum == 1) {

    if (!setalarm) {//ilk giriş bölümü
      setalarm = true;//alarmın devrede olduğunu söylüyor
      Serial.println("Alarm 1 Çalıyor");
      myMP3.volume(30);
      delay(20);
      myMP3.loop(1); //SD karttaki 2 numaralı dosyayı sürekli oynatıyor.
      delay(20);
    }

  }
  if ((alarmHour2 == actualHour) && (alarmMinute2 == actualMinute) && alarmdurum2 == 1) {

    if (!setalarm) {//ilk giriş bölümü
      setalarm = true;//alarmın devrede olduğunu söylüyor
      Serial.println("Alarm 2 Çalıyor");
      myMP3.volume(30);
      delay(20);
      myMP3.loop(2); //SD karttaki 2 numaralı dosyayı sürekli oynatıyor.
      delay(20);
    }

  }


}
//Alarmı durduracağımız fonksiyon.
void alarmdurdur() {
  Serial.println("Alarm Sona Erdi");
  myMP3.stop();//MP3 player durduruldu
  setalarm = false; //alarmın devrede olduğu bilgisi iptal edildi.
}


////////////////WEB SUNUCUSU FONKSİYONLARI/////////////////////
//sayfa isteklerini yöneten fonksiyonumuz
void handleMainPage() {
  String acik = ""; //alarm acık durumu için kullanacağız.
  String kapali = ""; //alarm kapalı durumu için
  String acik2 = ""; //alarm acık durumu için kullanacağız.
  String kapali2 = ""; //alarm kapalı durumu için
  String alarmHour_1 = ""; //alarm saati için
  String alarmMinute_1 = ""; //alarm dakikasi için
  String alarmHour_2 = ""; //alarm saati2 için
  String alarmMinute_2 = ""; //alarm dakikasi2 için

  if (alarmdurum == 1) {
    acik = "checked=\"checked\"";
  }
  else {
    kapali = "checked=\"checked\"";
  }
  if (alarmdurum2 == 1) {
    acik2 = "checked=\"checked\"";
  }
  else {
    kapali2 = "checked=\"checked\"";
  }
  webPage = "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head>";
  webPage += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />";
  webPage += "<title>ledsaat_server</title></head><body><p><font size=\"16\">LedSaat Sunucu Ayar Sayfası</font></p>";
  webPage += "<p>&nbsp;</p><form id=\"form1\" name=\"form1\" method=\"get\" action=\"/set\">";
  webPage += "<label>Alarm Süresi (sn olarak 1-59):<br />";
  webPage += "Alarm Süresi<input type=\"text\" name=\"alarmduration\" id=\"alarmduration\" /></label>";

  //alarm1 saat dakika
  webPage += "<p>Alarm Saati:<label><input name=\"alarmsaat\" type=\"text\" id=\"alarmsaat\" maxlength=\"2\" /></label></p>";
  webPage += "<p>Alarm Dakika:<label><input name=\"alarmdakika\" type=\"text\" id=\"alarmdakika\" maxlength=\"2\" /></label></p>";
  //alarm2 saat dakika
  webPage += "<p>Alarm 2 Saati:<label><input name=\"alarmsaat2\" type=\"text\" id=\"alarmsaat2\" maxlength=\"2\" /></label></p>";
  webPage += "<p>Alarm 2 Dakika:<label><input name=\"alarmdakika2\" type=\"text\" id=\"alarmdakika2\" maxlength=\"2\" /></label></p>";

  //radio butonlar
  webPage += "<p><label><input type=\"radio\" name=\"setalarm\" value=\"1\" id=\"setalarm_0\"" + acik + "/>";
  webPage += "Alarm-1 Açık</label><br /><label><input type=\"radio\" name=\"setalarm\" value=\"0\" id=\"setalarm_1\"" + kapali + " />";
  webPage += "Alarm-1 Kapalı</label> <br /> </p>";

  webPage += "<p><label><input type=\"radio\" name=\"setalarm2\" value=\"1\" id=\"setalarm2_0\"" + acik2 + "/>";
  webPage += "Alarm-2 Açık</label><br /><label><input type=\"radio\" name=\"setalarm2\" value=\"0\" id=\"setalarm2_1\"" + kapali2 + " />";
  webPage += "Alarm-2 Kapalı</label> <br /> </p>";

  //submit butonu
  webPage += "<p><input type=\"submit\" name=\"button\" id=\"button\" value=\"Onay\" />";

  webPage += "</p><p><label></label></p></form><p>&nbsp;</p><p>&nbsp;</p>";
  webPage += "Mevcut Alarm Saat Dakikalar<br>";
  if (alarmdurum == 1) { //eğer alarm özelliği aktif ise
    webPage += "Alarm-1<br>";
    if (alarmHour < 10) {
      alarmHour_1 = "0" + String(alarmHour);
    }
    else {
      alarmHour_1 = String(alarmHour);
    }
    if (alarmMinute < 10) {
      alarmMinute_1 = "0" + String(alarmMinute);
    }
    else {
      alarmMinute_1 = String(alarmMinute);
    }

    webPage += alarmHour_1 + ":";
    webPage += alarmMinute_1;
    webPage += "<br>Alarm Çalma Süresi:";
    webPage += alarmduration;
    webPage += "saniye</br>";
  }
  if (alarmdurum2 == 1) { //eğer alarm özelliği aktif ise
    if (alarmHour2 < 10) {
      alarmHour_2 = "0" + String(alarmHour2);
    }
    else {
      alarmHour_2 = String(alarmHour2);
    }
    if (alarmMinute2 < 10) {
      alarmMinute_2 = "0" + String(alarmMinute2);
    }
    else {
      alarmMinute_2 = String(alarmMinute2);
    }
    webPage += "<br>Alarm-2<br>";
    webPage += alarmHour_2 + ":";
    webPage += alarmMinute_2;
    webPage += "<br>Alarm Çalma Süresi:";
    webPage += alarmduration;
    webPage += "saniye</br>";
  }

  webPage += "</body></html>";

  webStat = "<p style=\"font-size: 90%; color: #FF8000;\">RSSI: ";
  webStat += WiFi.RSSI();
  webStat += "<br/>";
  webStat += "Uptime [min]: ";
  webStat += millis() / (1000 * 60);
  webStat += "</p>";
  webFooter = "<p style=\"font-size: 80%; color: #08088A;\">MucitPilot LedSaatWifi v1.0 | <a href=\"mailto:mucitpilot@gmail.com\">eposta gonderin:</a> | <a href=\"https://www.youtube.com/channel/UCIful2Qhus_avYGcUaC0JmQ\">Youtube Sayfam</a></p></body></html>";
  server.send(200, "text/html", webPage + webStat + webFooter);
  Serial.println("Web sayfası isteği");
}

//sayfadan gelen değişkenlerin alınıp kodun içinde kullanılması için kullandığımız fonksiyon
void handleSetCommand() {
  String response = "";
  if (server.args() == 0) { //hiç argüman gelmemişse
    response = "Parametre Yok";
    Serial.println("Parametre Yok");
  }
  else { //veri gelmişse
    if (server.argName(0) == "alarmduration") { //alarmduration isimli gelen veriyi al ve Integer'a çevirip programdaki değişkeni güncelle.
      //EPROM'a kaydet.

      alarmduration = server.arg("alarmduration").toInt();
      if (alarmduration == 0 || alarmduration >= 60) {
        alarmduration = 15;
      }
      Serial.println(alarmduration);
      EEPROM.write(101, alarmduration);
      EEPROM.commit();
      delay(100);
      //response = "Senkron süresi güncellendi:";
      //response += alarmduration;
    }


    if (server.argName(1) == "alarmsaat") {
      alarmHour = (byte)server.arg("alarmsaat").toInt();
      if (alarmHour >= 24) {
        alarmHour = 8;
      }
      Serial.println(alarmHour);
      EEPROM.write(103, alarmHour);
      EEPROM.commit();
      delay(100);
      //response = "Alarm saati: ";
      //response += alarmHour;
    }
    if (server.argName(2) == "alarmdakika") {
      alarmMinute = (byte)server.arg("alarmdakika").toInt();
      if (alarmMinute >= 60) {
        alarmMinute = 0;
      }
      Serial.println(alarmMinute);
      EEPROM.write(104, alarmMinute);
      EEPROM.commit();
      delay(100);


      //response += alarmMinute;
    }
    if (server.argName(3) == "alarmsaat2") {
      alarmHour2 = (byte)server.arg("alarmsaat2").toInt();
      if (alarmHour2 >= 24) {
        alarmHour2 = 8;
      }
      Serial.println(alarmHour2);
      EEPROM.write(105, alarmHour2);
      EEPROM.commit();
      delay(100);
      //response = "Alarm saati: ";
      //response += alarmHour;
    }

    if (server.argName(4) == "alarmdakika2") {
      alarmMinute2 = (byte)server.arg("alarmdakika2").toInt();
      if (alarmMinute2 >= 60) {
        alarmMinute2 = 0;
      }
      Serial.println(alarmMinute2);
      EEPROM.write(106, alarmMinute2);
      EEPROM.commit();
      delay(100);


      //response += alarmMinute;
    }

    if (server.argName(5) == "setalarm") {

      alarmdurum = server.arg("setalarm").toInt();

      Serial.println(alarmdurum);
      EEPROM.write(102, alarmdurum);
      EEPROM.commit();
      delay(100);
      //response = "Senkron süresi güncellendi:";
      //response += alarmduration;

    }
    if (server.argName(6) == "setalarm2") {

      alarmdurum2 = server.arg("setalarm2").toInt();

      Serial.println(alarmdurum2);
      EEPROM.write(107, alarmdurum2);
      EEPROM.commit();
      delay(100);
      //response = "Senkron süresi güncellendi:";
      //response += alarmduration;
      response = "<html><head><meta http-equiv=\"refresh\" content=\"2; url=/\"></head><body>Ayarlar Guncellendi. Bekleyin ana sayfaya yonlendiriliyorsunuz.</body></html>";
    }



    if (response == "" ) {
      response = "Hatalı Parametre";
    }
    server.send(200, "text/html", response);          //HTTP cevaplarını sunucuya gönderiyor.
    Serial.print("Change request: ");
    Serial.println(response);

  }
}

/////////Bu fonksiyon ile Donanımsal Interrupt(Kesme) tanımlamış oluyoruz. Bu sayede butona bastığımızda
//kodun neresinde olursa olsun hemen reaksiyon göstererek buton işlemleri gerçekleştirilmiş olur.
ICACHE_RAM_ATTR void Alarmstop() {
  if (!button) { //butona şu ana kadar hiç basılmamışsa yani ilk basışı ifade ediyoruz
    if (digitalRead(D1) == HIGH) { //ve D1 pini HIGH olmuş yani butona basılmışsa
      buttonSayac = millis(); //basma süresini takip edecek bir sayac tanımlıyoruz
      button = true; //ve butona basıldı bilgisini tutan değişkeni TRUE yapıyoruz.
    }
  } 
  else { // eğer butona daha öncesinde basılmış ise
    if (digitalRead(D1) == LOW) { //ve artık buton bırakılmış ise
      button = false; //butonu tekrar false a çek
      if ((millis() - buttonSayac) >= 50) { //eğer bu basma zarfında basma süresi 50ms den büyük ise hareketi kaale al ve
        if ((millis() - buttonSayac) >= 3000) { toplam basma süresi 3sn den fazla sürmüş ise
          Serial.println(">3 sn");
          resetle = true;   //resetleme işleminin gerçekleştiğini güncelle ve TRUE yap
          resetecek=true; //yukarıdakinin benzeri bir kullanım için
        } else {       //////3sn den az bir basma olmuş ise
          alarmdurdur(); //bunu sadece alarm durdurma işlemi olarak kullan
          sayac = 0;  //işimiz bitince de buton süre sayacını sıfırla
          Serial.println("<3 sn");
        }
      }
    }
  }

}
