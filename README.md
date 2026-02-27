# Herná konzola
![Fotka hernej konzoly](Photos_amd_videos/Zapojenie_v_obale.jpg)
Cieľom bolo vytvoriť kompaktné zariadenie, ktoré spája retro herný zážitok s modernou elektronikou. 
Konzola disponuje vlastným grafickým používateľským rozhraním (menu) a štyrmi hrateľnými hrami.
## Technické špecifikácie
* **Mikrokontrolér:** ESP32 (výkonnejší nástupca pôvodného Arduino UNO).
* **Zobrazenie kódu:** OLED displej s rozlíšením 128x64 px (I2C komunikácia).
* **Ovládanie:** 2 analógové joysticky pre plynulé hranie.
* **Zvuk:** Pasívny bzučiak pre retro zvukové efekty.
* **Napájanie:** 3.7 V batéria s vypínačom a nabíjacím modulom TP4056 s USB-C pre mobilitu.
* **Obal konzoly:** Vyrobený tak aby umožnil vloženie komponentov na breadboard a bezproblémové ovládanie konzoly pomocou joystikov.
## Hry
* **Pong:** Klasická stolná hra pre dvoch hráčov.
* **Breakout:** Rozbíjanie tehličiek loptičkou.
* **MazeRunner:** Hľadanie cesty v bludisku (Bludisko sa stále mení).
* **TicTacToe:** Logická hra **Piškvorky** pre jedného a dvoch hráčov.
## Potrebné knižnice pre kompiláciu (Arduino IDE):
* **Adafruit GFX Library**
* **Adafruit SH110X**
* **U8g2** by oliver <<olikraus@gmail.com>> 
