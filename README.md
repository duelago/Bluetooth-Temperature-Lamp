# <h1>Blåtandstermometerlampan</h1>

En bordslampa som byter färg beroende på yttertemperaturen och som även visar tempen på en liten OLED-skärm.
Lampan kommunicerar med en billig blåtandstermometer som vi installerar alternativ firmware på och placerar utomhus (i närheten).
Kodat i Arduino IDE. Termometern vi ansluter till måste flashas en gång med hjälp av ett pythonscript och lödkolven måste fram. 
<p></p>
Jag printar lådan och locket i PLA med 0.6 nozzle på min Prusa.<br>
Själva toppen av lampan till höger i bild är printad med 0.4 nozzle. Två perimetrar, ingen infill i själva lampan men fästet längst ner har infill. (Prusa slicer har stöd för olika infill på olika lager) Pinnen är ett metallsugrör från ICA<p></p>

![Screenshot 2024-11-10 at 17 25 36 copy](https://github.com/user-attachments/assets/56b19ddc-1d22-4644-b5cb-de2b17a1d4d6)



![bluetoothesp32](https://github.com/user-attachments/assets/33bab323-7351-49cc-a0c5-6bc6c0fe055e)



<h2>Själva hjärnan i operationen är en ESP32</h2>

Detta behövs:<br>
ESP32 38 pin- Testad med denna modellen från AliExpress https://www.aliexpress.com/item/1005006900641832.html<p></p> (Jag kör med micro usb-anslutning eftersom jag har ett rejält lager med sådana kablar)
Blåtandstermometer - https://www.aliexpress.com/item/1005006458594110.html<p></p>
LED  - https://www.aliexpress.com/item/1005004950092116.html<p></p>
OLED-skärm https://www.aliexpress.com/item/32804426981.html<p></p>
USB-to-TTL adapter (Vet inte vilken som är bra, men typ denna) https://www.aliexpress.com/item/1005004742270942.html<p></p>
Metallsugrör från ICA eller Amazon https://t.ly/_2JY1<p></p>
1 st motstånd 100 Ohm<p></p>
2 skruvar M2.5 <p></p>
1 micro usb-kabel<p></p>
Krympslang till LEDen https://www.aliexpress.com/item/1005006991396293.html <p></p>
Dupontkablar (female-female) 3st 30cm  och ett gäng 10cm https://www.aliexpress.com/item/1005005501503609.html<p></p>

3D-printade detaljer i PLA<p></p>

![Screenshot 2024-11-10 at 20 31 09](https://github.com/user-attachments/assets/8a75f3b7-e314-4ef4-acd3-05346076fb09)


![Screenshot 2024-11-09 at 18 48 15](https://github.com/user-attachments/assets/5eb7eb65-e5cb-4701-9203-0e4e120e31b3)

![Screenshot 2024-11-09 at 18 47 59](https://github.com/user-attachments/assets/badcc2b1-c37b-4668-998f-14636b6fe3af)

![Screenshot 2024-11-09 at 18 50 36](https://github.com/user-attachments/assets/3c1b2925-759c-4cb8-ba95-4a009e5b4c22)

![Screenshot 2024-11-09 at 18 54 38](https://github.com/user-attachments/assets/704f6198-a035-4996-8708-9e186b75ae49)


<h2>Steg ett är att flasha firmware på vår termometer</h2>
Plocka isär den och löd fast kablar enligt denna bild. Dessa ska du sedan koppla till din USB-to-TTL adapter<p></p>
<b>rx-tx<br>
tx-rx<br>
vbat+- 3.3v<br>
vbat- - ground<br>
rest - reset</b><p></p>

![bth01](https://github.com/user-attachments/assets/803ec53a-a5db-4e93-a42c-f810dcf24239)
<p></p>
Följ instruktionerna här för hur du flashar din BTH01  https://github.com/pvvx/THB2 <br> (Denna modellen har jag, men det finns varianter https://pvvx.github.io/BTH01/ )<br>

Först måste du flasha .hex filen via terminalen med hjälp av python scriptet. (Finns även bland mina filer) <br>
Typ så här görs det på min mac:<p>

![Screenshot 2024-11-10 at 17 37 44](https://github.com/user-attachments/assets/9c286a5a-9f0a-4463-80fe-4490a1844e3b)
Din COM-port heter något annat. Den får du luska reda på.
<h2>Web-interfacet där du uppdaterar din firmware i fortsättningen ser ut så här</h2>


![Screenshot 2024-11-10 at 10 51 43](https://github.com/user-attachments/assets/f4c2ec4e-3faa-4bb0-8a1c-c26970b5aea2)
<p></p>
När du flashat klart, klickar du på  READ under config-taben och ta reda på termometerns MAC-adress. Denna adress klistrar du in i .ino filen före du kompilerar lampans firmware
<p></p>
Sen är det bara att kompilera [.ino](https://github.com/duelago/bluetoothlamp/blob/main/bluetooth-esp32.ino) filen i Arduino IDE. Se till att korrekt esp32 är vald som board och att alla bibliotek är installerade.
<p></p>

![Screenshot 2024-11-10 at 17 43 17](https://github.com/user-attachments/assets/c31c2913-b47f-4bf1-b265-2f90f9bb2758)

<h2>nRF Connect</h2>

Tips. Denna appen är utmärkt för att se mer info om din blåtandstermometer samt för felsökning. Klarar av att decoda temperaturen även när vi kör custom firmware<p></p>
![Screenshot_2024-11-11-08-14-45-61_b5a5c5cb02ca09c784c5d88160e2ec24](https://github.com/user-attachments/assets/37599e6a-c099-4c1a-941f-a13828a8cf3f)

<h1>Snöflingelampan</h1>

https://www.youtube.com/watch?v=6aXL863onwM
<img width="1728" alt="Screenshot 2024-11-14 at 14 47 33" src="https://github.com/user-attachments/assets/d1093373-c1f8-4ac1-91b9-4fd7299f5081">

![bluetooth](https://github.com/user-attachments/assets/fac4d198-dc57-4b21-b2d4-c6e285787fcd)

Denna co2-sensorn, Scd40 använder jag i snöflingan. Dyr men bra.
https://www.aliexpress.com/item/1005006275318058.html
I övrigt är det bara att följa instruktionerna för blåtandslampan. Skillnaden är att vi måste seriekoppla 6 leds. Hur det görs ser du på skissen. Data in till data ut. Co2-sensorn kan köras på både 3.3V och 5V, men 5 volt rekommenderas.

<img width="1316" alt="Screenshot 2024-11-14 at 18 16 51" src="https://github.com/user-attachments/assets/9654d1a4-1387-418b-aa64-a086f973d8fc">






-----
<br>
Mer dokumentation om blåtandsgrunkorna<br> 
https://github.com/pvvx/ATC_MiThermometer#readme
