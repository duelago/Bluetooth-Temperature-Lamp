# Blåtandslampan

En bordslampa som byter färg beroende på yttertemperaturen och som även visar tempen op en liten OLED-skärm
Lampan kommunicerar med en billig blåtandstermometer som vi installerar alternativ firmware på.

Själva hjärnan i operationen är en ESP32

Detta behövs:
ESP32 38 pin- Testad med denna modellen från AliExpress https://www.aliexpress.com/item/1005006900641832.html
Blåtandstermometer - https://www.aliexpress.com/item/1005006458594110.html
LED  - https://www.aliexpress.com/item/1005004950092116.html
OLED-skärm https://www.aliexpress.com/item/32804426981.htm
USB-to-TTL adapter (Vet inte vilken som är bra, men typ denna) https://www.aliexpress.com/item/1005004742270942.html
Dupontkablar
Metallsugrör från ICA eller Amazon

![Screenshot 2024-11-09 at 18 48 34](https://github.com/user-attachments/assets/58aed1f7-5daf-4fa9-8259-60117b535dff)

![Screenshot 2024-11-09 at 18 48 15](https://github.com/user-attachments/assets/5eb7eb65-e5cb-4701-9203-0e4e120e31b3)

![Screenshot 2024-11-09 at 18 47 59](https://github.com/user-attachments/assets/badcc2b1-c37b-4668-998f-14636b6fe3af)

![Screenshot 2024-11-09 at 18 50 36](https://github.com/user-attachments/assets/3c1b2925-759c-4cb8-ba95-4a009e5b4c22)

![Screenshot 2024-11-09 at 18 54 38](https://github.com/user-attachments/assets/704f6198-a035-4996-8708-9e186b75ae49)


Steg ett är att flasha firmware på vår termometer. Plocka isär den och löd fast kablar enligt denna bild. Dessa ska du sedan koppla till din USB-to-TTL adapter
rx-tx
tx-rx
vbat+- 3.3v
vbat- - ground
rest - reset

![bth01](https://github.com/user-attachments/assets/803ec53a-a5db-4e93-a42c-f810dcf24239)

Följ instruktionerna här för hur du flashar
https://github.com/pvvx/THB2



Börja här: https://github.com/pvvx/ATC_MiThermometer#readme
