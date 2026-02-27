/*
 * @file Kod_konzoly.ino
 * @brief Systém viacerých hier pre ESP32 (Pong, Breakout, MazeRunner, TicTacToe)
 * @details Tento projekt implementuje 4 klasické hry na platforme ESP32 s využitím 
 * OLED displeja SH1106. Projekt využíva hybridné použitie grafických knižníc 
 * (Adafruit GFX & U8g2) pre optimalizáciu vykresľovania rôznych typov hier.
 * @notes Hardware: ESP32 Dev Module, OLED SH1106 (I2C), Joystick, Buzzer
 * @author [Peter Marcinčák] - SOČ
 */

/*
 * @brief Poznámka k vývoju kódu
 * @notes V procese programovania kódu pre pôvodný prototyp fungujúci na 
 * Arduino UNO R3 sme zistili, že ak píšeme podobné príkazy do riadku
 * za sebou a nie každý príkaz do samostatného riadku, výsledná veľkosť
 * kódu je vždy o niekoľko % menšia. Ani to však nestačilo na to, aby
 * sa kód zmestil na Arduino UNO R3, preto sme sa rozhodli prejsť na
 * vývojovú dosku ESP32, ktorá umožnovala ponechať kód len s minimálnymi
 * zmenami.
 */

/* --- Inklúzia potrebných knižníc --- */
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>       // Grafické jadro pre Pong, Breakout, Maze
#include <Adafruit_SH110X.h>    // Ovládač pre SH1106 displej (Adafruit stack)
#include <Fonts/FreeSans9pt7b.h>
#include <U8g2lib.h>            // Grafická knižnica použitá špecificky pre TicTacToe (lepšie fonty/primitives)

// =================================================================
// Konfigurácia hardvéru a displeja
// =================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define BLACK 0
#define WHITE 1

/** Objekt pre ovládanie displeja cez Adafruit knižnicu (I2C adresa 0x3C) */
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/** Objekt pre ovládanie displeja cez U8g2 knižnicu (Hardvérová akcelerácia I2C) */
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// =================================================================
// Mapovanie GPIO pinov (ESP32)
// =================================================================
const int SW_pin    = 33; // Primárne tlačidlo (Potvrdenie/Menu)
const int SW2_pin   = 32; // Sekundárne tlačidlo (Návrat/Reset)
const int PLAYER2_Y_AXIS = 35; // Analógový vstup pre druhého hráča (Pong)
const int JOY2_VRX  = 36; // Joystick X-os
const int JOY2_VRY  = 39; // Joystick Y-os
const int BEEPER    = 27; // Pasívny buzzer pre zvukovú odozvu

// Aliasy pre ovládanie TicTacToe (pre lepšiu čitateľnosť v logike hry)
#define BUTTON_MOVE SW_pin 
#define BUTTON_OK   SW2_pin

// =================================================================
// Globálne premenné - Správa stavu aplikácie
// =================================================================
int selectedGame = 0; // Index vybranej hry: 0:Pong, 1:Breakout, 2:Maze, 3:TicTacToe
bool inMenu = true;   // Flag určujúci, či sa nachádzame v hlavnom menu
bool soundOn = true;  // Globálne nastavenie zvuku

// =================================================================
// PONG - Premenné a konštanty
// =================================================================
const unsigned long PADDLE_RATE = 45; // Rýchlosť obnovovania rakiet (ms)
const uint8_t PADDLE_HEIGHT = 12, BALL_SIZE = 2, PLAYER2_X = 22, PLAYER_X = 105;
int player1Score = 0, player2Score = 0, maxScore = 10;
bool resetBall = true;
uint8_t ball_x = 64, ball_y = 32, player2_y = 26, player1_y = 26;
int8_t ball_dir_x = 1, ball_dir_y = 1; // Vektor smeru loptičky
unsigned long ball_update = 0, paddle_update = 0; // Časovače pre "non-blocking" delay

// =================================================================
// BREAKOUT - Premenné a konštanty
// =================================================================
// Prahové hodnoty pre joystick (kalibrácia stredu)
#define Data_of_left 1200
#define Data_of_right 2600
int WX; byte WL; // Pozícia a šírka pádla (Paddle Width/Location)
float BX, BY, AX, AY; // Pozícia lopty (Ball X/Y) a akcelerácia/smer (Acceleration X/Y)
boolean MAP[16][8], WIN; // Matica tehličiek (16 stĺpcov, 8 riadkov)

// =================================================================
// MAZERUNNER - Premenné pre generovanie a renderovanie
// =================================================================
#define CELL_SIZE 8
const int MAZE_COLS = 32;
const int MAZE_ROWS = 16;
// Výpočet viditeľnej oblasti (Viewport) na základe veľkosti displeja
const int VIEW_COLS = SCREEN_WIDTH / CELL_SIZE;
const int VIEW_ROWS = SCREEN_HEIGHT / CELL_SIZE;
uint8_t maze[MAZE_ROWS][MAZE_COLS]; // 2D pole reprezentujúce bludisko (1=stena, 0=cesta)
int playerX = 1, playerY = 1;
int viewX = 0, viewY = 0; // Offset kamery

// =================================================================
// TIC-TAC-TOE - Logika umelej inteligencie a stav hry
// =================================================================
int board[] = {0,0,0, 0,0,0, 0,0,0};  // Reprezentácia hracej plochy (0=prázdne, 1=Hráč, 2=CPU)
int gameStatus = 0;  
int whosplaying = 1; // 1 = Hráč, 2 = CPU/Hráč 2
int winner = 0; 
int mode = 0; // 0 = vs CPU, 1 = PvP
int playerSymbol = 1;
int cpuSymbol = 2;

// =================================================================
// PROTOTYPY FUNKCIÍ
// (Deklarácia dopredu kvôli kompresii kódu a prehľadnosti)
// =================================================================
void showMenu(); void countdown();
// Pong
void playPong(); void drawCourt(); void drawScore(); void gameOverPong(); void soundBounce(); void soundPoint();
// Breakout
void playBreakout(); void startBreakout(); void drawBreakout(); void winBreakout(); void failBreakout();
// MazeRunner
void startMazeRunner(); void playMazeRunner(); void updateViewport(); void drawScene(); void attemptMove(int dx,int dy);
void showWinScreen(); void showPlayAgainMenu(); void showMiniMap(); void carveMaze(int x, int y); void generateMaze();
// TicTacToe
void playTicTacToe(); void showMenu_TicTacToe(); void choosePlayerSymbol(); void boardDrawing(); 
void drawBoardWithCursor(int cursorPos); void showTurnIndicator(); void showWinner_TicTacToe(int winnerSymbol); 
bool playAgainMenu_TicTacToe(); void playhuman(int symbol); void playcpu(); int checkboard(int x); 
void checkWinner(); int findWinningMove(int symbol); int findBlockingMove(int symbol); const char* charBoard(int x);

// =================================================================
// SETUP - Inicializácia systému
// =================================================================
void setup() {
  // Inicializácia I2C zbernice na definovaných pinoch pre ESP32
  Wire.begin(21, 22);

  // Konfigurácia vstupov a výstupov
  pinMode(SW_pin, INPUT_PULLUP);
  pinMode(SW2_pin, INPUT_PULLUP);
  pinMode(BEEPER, OUTPUT);
  pinMode(PLAYER2_Y_AXIS, INPUT);
  pinMode(JOY2_VRX, INPUT);
  pinMode(JOY2_VRY, INPUT);

  // Inicializácia Adafruit displeja (OLED Driver)
  if (!display.begin(0x3C, true)) { /* Error handling - slučka v prípade chyby */ }
  display.clearDisplay();
  display.display();
  
  // Inicializácia U8g2 displeja (pre použitie v TicTacToe)
  u8g.begin(); 

  // Seedovanie generátora náhodných čísel šumom z nezapojeného analógového pinu
  randomSeed(analogRead(34)); 
  showMenu();
}

// =================================================================
// HLAVNÁ SLUČKA (Main Loop)
// =================================================================
void loop() {
  if (inMenu) {
    showMenu(); // Vykresľovanie a logika hlavného menu
  } else {
    // Stavový automat pre prepínanie medzi jednotlivými hrami
    switch (selectedGame) {
      case 0: playPong(); break;
      case 1: playBreakout(); break;
      case 2: playMazeRunner(); break;
      case 3: playTicTacToe(); break;
      default: inMenu = true; break;
    }
  }
}

// =================================================================
// SYSTÉM MENU
// =================================================================
/*
 * @brief Vykresľuje hlavné menu a spracováva výber hry
 */
void showMenu() {
  display.setFont(NULL);
  display.setTextSize(1);
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(WHITE);

  // Navigácia v menu pomocou analógového vstupu (joystick/potenciometer)
  analogRead(PLAYER2_Y_AXIS); 
  int joy = analogRead(PLAYER2_Y_AXIS) / 4;

  static unsigned long lastMove = 0;
  unsigned long now = millis();
  const int totalGames = 4;

  // Debouncing a spracovanie vstupu pre posun v menu
  if (now - lastMove > 150) {
    if (joy < 400) selectedGame = (selectedGame + totalGames - 1) % totalGames; 
    else if (joy > 600) selectedGame = (selectedGame + 1) % totalGames; 
    lastMove = now;
  }

  const char *menuItems[] = { "Pong", "Breakout", "MazeRunner", "TicTacToe" };

  // Vykreslenie položiek menu s kurzorom
  for (int i = 0; i < totalGames; i++) {
    display.setCursor(5, i * 15 + 12); 
    display.print(i == selectedGame ? "> " : "  ");
    display.print(menuItems[i]);
  }

  display.display();
  delay(50);

  // Potvrdenie výberu tlačidlom
  if (digitalRead(SW_pin) == LOW || digitalRead(SW2_pin) == LOW) {
    delay(200);
    inMenu = false;
    display.clearDisplay();
    display.display();

    // Inicializácia parametrov pre vybranú hru pred jej spustením
    if (selectedGame == 0) { // Pong
      player1Score = player2Score = 0;
      resetBall = true;
      player1_y = player2_y = 26;
      countdown();
    } 
    else if (selectedGame == 1) { // Breakout
      startBreakout();
      countdown();
    } 
    else if (selectedGame == 2) { // MazeRunner
      startMazeRunner();
      countdown();
    }
    else if (selectedGame == 3) { // TicTacToe
      showMenu_TicTacToe(); 
      choosePlayerSymbol();
      whosplaying = 1;
      gameStatus = 0; 
    }
  }
}

/*
 * @brief Zobrazí odpočet 3-2-1 pred začiatkom hry
 */
void countdown() {
  display.setFont(NULL);
  display.setTextSize(6);
  for (int i = 3; i > 0; i--) {
    display.clearDisplay();
    display.setCursor((SCREEN_WIDTH - 36) / 2, (SCREEN_HEIGHT - 48) / 2);
    display.print(i);
    display.display();
    delay(1000);
  }
}

// =================================================================
// HRA PONG - Implementácia
// =================================================================
void playPong() {
  bool upd = false;
  unsigned long t = millis();

  // Reset loptičky po skórovaní
  if (resetBall) {
    if (player1Score == maxScore || player2Score == maxScore) {
      gameOverPong();
      inMenu = true;
      return;
    }
    display.clearDisplay();
    drawScore();
    drawCourt();
    // Náhodný smer a pozícia podania
    ball_x = random(45, 50);
    ball_y = random(23, 33);
    do { ball_dir_x = random(-1, 2); } while (!ball_dir_x);
    do { ball_dir_y = random(-1, 2); } while (!ball_dir_y);
    resetBall = false;
  }

  // Aktualizácia fyziky loptičky
  if (t > ball_update) {
    int nx = ball_x + ball_dir_x, ny = ball_y + ball_dir_y;
    
    // Detekcia skórovania (loptička mimo obrazovky vľavo/vpravo)
    if (nx <= 0) { player1Score++; if (soundOn) soundPoint(); resetBall = true; } 
    else if (nx + BALL_SIZE >= SCREEN_WIDTH) { player2Score++; if (soundOn) soundPoint(); resetBall = true; }

    // Odraz od hornej a dolnej steny
    if (ny <= 0 || ny + BALL_SIZE >= SCREEN_HEIGHT) { if (soundOn) soundBounce(); ball_dir_y = -ball_dir_y; ny = ball_y + ball_dir_y; }

    // Detekcia kolízií s raketami (Simple AABB collision logic)
    if (nx <= PLAYER2_X + 1 && nx + BALL_SIZE >= PLAYER2_X && ny + BALL_SIZE >= player2_y && ny <= player2_y + PADDLE_HEIGHT) {
      if (soundOn) soundBounce(); ball_dir_x = -ball_dir_x; nx = ball_x + ball_dir_x;
    }
    if (nx + BALL_SIZE >= PLAYER_X && nx <= PLAYER_X + 1 && ny + BALL_SIZE >= player1_y && ny <= player1_y + PADDLE_HEIGHT) {
      if (soundOn) soundBounce(); ball_dir_x = -ball_dir_x; nx = ball_x + ball_dir_x;
    }

    // Prekreslenie loptičky (vymazanie starej, nakreslenie novej)
    display.fillRect(ball_x, ball_y, BALL_SIZE, BALL_SIZE, BLACK);
    display.fillRect(nx, ny, BALL_SIZE, BALL_SIZE, WHITE);
    ball_x = nx; ball_y = ny; ball_update = t + 5; upd = true;
  }

  // Aktualizácia pohybu rakiet
  if (t > paddle_update) {
    paddle_update += PADDLE_RATE;
    // P2 Paddle (AI alebo Player 2)
    display.drawFastVLine(PLAYER2_X, player2_y, PADDLE_HEIGHT, BLACK);
    analogRead(PLAYER2_Y_AXIS); 
    if (analogRead(PLAYER2_Y_AXIS) < 1900) player2_y--; else if (analogRead(PLAYER2_Y_AXIS) > 2400) player2_y++;
    player2_y = constrain(player2_y, 1, SCREEN_HEIGHT - PADDLE_HEIGHT);
    display.drawFastVLine(PLAYER2_X, player2_y, PADDLE_HEIGHT, WHITE);

    // P1 Paddle (Hlavný hráč)
    display.drawFastVLine(PLAYER_X, player1_y, PADDLE_HEIGHT, BLACK);
    if (analogRead(JOY2_VRX) < 1900) player1_y--; else if (analogRead(JOY2_VRX) > 2400) player1_y++;
    player1_y = constrain(player1_y, 1, SCREEN_HEIGHT - PADDLE_HEIGHT);
    display.drawFastVLine(PLAYER_X, player1_y, PADDLE_HEIGHT, WHITE);
    upd = true;
  }

  if (upd) {
    drawScore();
    display.display();
    // Návrat do menu
    if (digitalRead(SW_pin) == LOW || digitalRead(SW2_pin) == LOW) { inMenu = true; delay(300); }
  }
}

void drawCourt() { display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE); }
void drawScore() {
  display.setFont(NULL); display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(45, 3); display.print(player2Score);
  display.setCursor(75, 3); display.print(player1Score);
}
void gameOverPong() {
  display.clearDisplay(); display.setFont(NULL); display.setTextSize(2);
  display.setCursor(20, 15); display.print((player1Score > player2Score) ? "Player 1" : "Player 2");
  display.setCursor(47, 35); display.print("won");
  display.display(); delay(2000);
}
void soundBounce() { tone(BEEPER, 500, 50); }
void soundPoint() { tone(BEEPER, 100, 50); }

// =================================================================
// HRA BREAKOUT - Implementácia
// =================================================================
void startBreakout() {
  WL = 32; WX = (SCREEN_WIDTH - WL) / 2;
  BX = SCREEN_WIDTH / 2; BY = 52; AX = 4; AY = -4; WIN = false;
  // Inicializácia poľa tehličiek
  for (int x = 0; x < 16; x++) for (int y = 0; y < 8; y++) MAP[x][y] = true;
}

void playBreakout() {
  // Pohyb pádla joystickom
  int j = analogRead(JOY2_VRY);
  if (j < Data_of_left) WX += 5; else if (j > Data_of_right) WX -= 5;
  WX = constrain(WX, 0, SCREEN_WIDTH - WL);

  // Fyzika lopty
  BX += AX; BY += AY;
  // Odraz od stien
  if (BX <= 0 || BX >= SCREEN_WIDTH - 3) { AX = -AX; if (soundOn) soundBounce(); BX = constrain(BX, 0, SCREEN_WIDTH - 3); }
  if (BY <= 0) { BY = 0; AY = -AY; if (soundOn) soundBounce(); }
  
  // Kolízia s pádlom hráča
  if (BY >= 53 && BY <= 56 && (BX + 3) >= WX && BX <= WX + WL) {
    BY = 53; AY = -AY; float hitPos = ((BX + 1.5) - (WX + WL / 2)) / (WL / 2);
    AX = hitPos * 3; if (soundOn) soundBounce();
  }

  // Prehra (Lopta spadla dole)
  if (BY > SCREEN_HEIGHT) { failBreakout(); return; }

  // Detekcia kolízie s tehličkami (Grid based collision)
  int bxI = int(BX) / 8; int byI = int(BY) / 4;
  if (bxI >= 0 && bxI < 16 && byI >= 0 && byI < 8 && MAP[bxI][byI]) {
    MAP[bxI][byI] = false; AY = -AY; if (soundOn) soundBounce();
  }

  // Kontrola podmienky výhry (všetky tehličky zničené)
  WIN = true;
  for (int x = 0; x < 16; x++) for (int y = 0; y < 8; y++) if (MAP[x][y]) { WIN = false; break; }
  if (WIN) { winBreakout(); return; }

  // Prerušenie hry
  if (digitalRead(SW_pin) == LOW || digitalRead(SW2_pin) == LOW) { inMenu = true; delay(300); return; }
  drawBreakout();
}

void drawBreakout() {
  display.clearDisplay();
  display.fillRect(WX, 56, WL, 3, WHITE); // Pádlo
  display.fillRect((int)BX, (int)BY, 3, 3, WHITE); // Lopta
  // Vykreslenie aktívnych tehličiek
  for (int x = 0; x < 16; x++) for (int y = 0; y < 8; y++) if (MAP[x][y]) display.fillRect(x * 8, y * 4, 7, 3, WHITE);
  display.display();
}

void winBreakout() {
  display.clearDisplay(); display.setFont(NULL); display.setTextSize(2);
  display.setCursor(12, 27); display.print("You Win!"); display.display(); delay(2000); inMenu = true;
}
void failBreakout() {
  display.clearDisplay(); display.setFont(NULL); display.setTextSize(2);
  display.setCursor(12, 27); display.print("Game Over"); display.display(); delay(2000); inMenu = true;
}

// =================================================================
// HRA MAZERUNNER - Implementácia
// =================================================================

/*
 * @brief Rekurzívna funkcia na generovanie bludiska (Algoritmus Recursive Backtracker)
 */
void carveMaze(int x, int y) {
  const int dirs[4][2] = {{2,0},{-2,0},{0,2},{0,-2}};
  int order[4] = {0,1,2,3};
  // Náhodné premiešanie smerov pre zabezpečenie náhodnosti bludiska
  for(int i=0;i<4;i++){ int r = random(i,4); int t=order[i]; order[i]=order[r]; order[r]=t; }
  
  for(int i=0;i<4;i++){
    int nx = x + dirs[order[i]][0]; int ny = y + dirs[order[i]][1];
    if(nx>0 && nx<MAZE_COLS-1 && ny>0 && ny<MAZE_ROWS-1){
      if(maze[ny][nx]==1){ // Ak je bunka stena (nenavštívená)
        maze[ny][nx]=0; maze[y + (ny-y)/2][x + (nx-x)/2]=0; // Zbúraj stenu medzi bunkami
        carveMaze(nx, ny); // Rekurzívne volanie
      }
    }
  }
}
void generateMaze() {
  // Vyplnenie plochy stenami
  for(int y=0;y<MAZE_ROWS;y++) for(int x=0;x<MAZE_COLS;x++) maze[y][x]=1;
  maze[1][1]=0; carveMaze(1,1);
  // Zabezpečenie prístupu k cieľu
  int goalX = MAZE_COLS-2; int goalY = MAZE_ROWS-2;
  maze[goalY][goalX] = 0; if(maze[goalY][goalX-1]==1 && maze[goalY-1][goalX]==1) maze[goalY][goalX-1] = 0;
}
void startMazeRunner() { generateMaze(); playerX = 1; playerY = 1; updateViewport(); }

void playMazeRunner() {
  static unsigned long lastMoveTime = 0; const unsigned long moveInterval = 80;
  static bool lastMiniMapPress = HIGH;
  
  // Zobrazenie minimapy
  bool miniMapPressed = digitalRead(SW_pin); 
  if (miniMapPressed == LOW && lastMiniMapPress == HIGH) showMiniMap();
  lastMiniMapPress = miniMapPressed;

  if (digitalRead(SW2_pin) == LOW) { inMenu = true; delay(300); return; }

  // Spracovanie pohybu (krokový pohyb)
  if(millis() - lastMoveTime > moveInterval){
    lastMoveTime = millis();
    int dx=0, dy=0;
    int joyX = analogRead(JOY2_VRX); int joyY = analogRead(JOY2_VRY);
    if(joyX<1400) dy=-1; else if(joyX>2600) dy=1;
    if(joyY<1400) dx=1; else if(joyY>2600) dx=-1;
    if(dx && dy) dy=0; // Zákaz diagonálneho pohybu
    if(dx || dy) attemptMove(dx,dy);
  }
  drawScene();
}

// Výpočet pozície kamery tak, aby bol hráč v strede (ak je to možné)
void updateViewport() {
  viewX = constrain(playerX - VIEW_COLS/2, 0, MAZE_COLS - VIEW_COLS);
  viewY = constrain(playerY - VIEW_ROWS/2, 0, MAZE_ROWS - VIEW_ROWS);
}
void drawScene() {
  display.clearDisplay();
  // Vykreslenie len viditeľnej časti bludiska (Optimalizácia)
  for(int row=0; row<VIEW_ROWS; row++){
    for(int col=0; col<VIEW_COLS; col++){
      int mx = viewX + col; int my = viewY + row;
      if(maze[my][mx]==1) display.fillRect(col*CELL_SIZE,row*CELL_SIZE,CELL_SIZE,CELL_SIZE,WHITE);
    }
  }
  // Vykreslenie hráča
  int px = (playerX-viewX)*CELL_SIZE+2; int py = (playerY-viewY)*CELL_SIZE+2;
  display.fillRect(px,py,CELL_SIZE-4,CELL_SIZE-4,WHITE);
  
  // Vykreslenie cieľa
  int goalX = MAZE_COLS-2; int goalY = MAZE_ROWS-2;
  if(goalX>=viewX && goalX<viewX+VIEW_COLS && goalY>=viewY && goalY<viewY+VIEW_ROWS){
    int gx = (goalX-viewX)*CELL_SIZE+2; int gy = (goalY-viewY)*CELL_SIZE+2;
    display.drawRect(gx,gy,CELL_SIZE-4,CELL_SIZE-4,WHITE);
  }
  display.display();
}
// Logika kolízie so stenami
void attemptMove(int dx,int dy){
  int nx = playerX + dx; int ny = playerY + dy;
  if(nx<0 || nx>=MAZE_COLS || ny<0 || ny>=MAZE_ROWS) return;
  if(maze[ny][nx]==1) return; // Kolízia so stenou
  playerX = nx; playerY = ny; updateViewport(); tone(BEEPER,1000,30);
  if(playerX==MAZE_COLS-2 && playerY==MAZE_ROWS-2){ showWinScreen(); showPlayAgainMenu(); }
}

void showWinScreen() {
  display.clearDisplay(); display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(22,20); display.print("You Win!"); display.display();
  tone(BEEPER,1500,200); delay(250); tone(BEEPER,2000,300); delay(2000);
}
void showPlayAgainMenu() {
  bool choice = true; bool lastPress = HIGH;
  while (true) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(28,14); display.print("Play again?");
    display.setCursor(40,30); display.print(choice ? "> YES <" : "  YES  ");
    display.setCursor(40,42); display.print(choice ? "  NO  " : "> NO  <");
    display.display();
    int joyX = analogRead(JOY2_VRX);
    if (joyX < 1400) { choice = true; tone(BEEPER,1200,50); delay(200); }
    else if (joyX > 2600) { choice = false; tone(BEEPER,1000,50); delay(200); }
    if (digitalRead(SW2_pin) == LOW && lastPress == HIGH) {
      tone(BEEPER,800,100); delay(200);
      if (choice) { generateMaze(); playerX = 1; playerY = 1; updateViewport(); return; }
      else { inMenu = true; return; }
    }
    lastPress = digitalRead(SW2_pin); delay(50);
  }
}
void showMiniMap() {
  display.clearDisplay(); const int scale = 4;
  for (int y = 0; y < MAZE_ROWS; y++) for (int x = 0; x < MAZE_COLS; x++) if (maze[y][x] == 1) display.fillRect(x * scale, y * scale, scale, scale, WHITE);
  int px = playerX * scale + scale / 2; int py = playerY * scale + scale / 2;
  display.fillCircle(px, py, 5, WHITE); display.drawCircle(px, py, 5, BLACK);
  display.drawCircle(px, py, 2, SH110X_BLACK); display.fillCircle(px, py, 2, SH110X_BLACK);
  display.display(); tone(BEEPER, 1400, 80); delay(2000);
}

// =================================================================
// HRA TIC-TAC-TOE (Piškvorky)
// Poznámka: Využíva knižnicu U8G2 pre odlišný grafický štýl
// =================================================================
void playTicTacToe() {
  if (gameStatus == 0) { // Inicializácia novej hry
    for (int i = 0; i < 9; i++) board[i] = 0;
    winner = 0; gameStatus = 1; boardDrawing();
  }

  if (gameStatus == 1) { // Hlavná slučka hry
    winner = 0;
    while (winner == 0) {
      if (whosplaying == 1) {
        playhuman(playerSymbol); whosplaying = 2;
      } else {
        if (mode == 0) { delay(300); playcpu(); } // Ťah AI
        else { playhuman(cpuSymbol); } // Ťah druhého hráča
        whosplaying = 1;
      }

      boardDrawing(); delay(300); checkWinner();

      if (winner > 0) {
        showWinner_TicTacToe(winner); delay(1500);
        bool again = playAgainMenu_TicTacToe();
        if (!again) { inMenu = true; gameStatus = 0; break; }
        gameStatus = 0; break;
      }
    }
  }
}

void showMenu_TicTacToe() {
  int selection = 0;
  // Výber módu (PvE alebo PvP) pomocou U8G2 vykresľovania (Page buffer loop)
  while (true) {
    u8g.firstPage();
    do {
      u8g.setFont(u8g2_font_6x10_tr); u8g.drawStr(0, 10, "Select Game Mode:");
      u8g.setFont(u8g2_font_9x15_tr);
      if (selection == 0) { u8g.drawStr(0, 30, "> CPU"); u8g.drawStr(0, 50, "  2 Players"); }
      else { u8g.drawStr(0, 30, "  CPU"); u8g.drawStr(0, 50, "> 2 Players"); }
    } while (u8g.nextPage());

    if (digitalRead(BUTTON_MOVE) == LOW) { selection = !selection; while (digitalRead(BUTTON_MOVE) == LOW); delay(200); }
    if (digitalRead(BUTTON_OK) == LOW) { while (digitalRead(BUTTON_OK) == LOW); delay(200); break; }
    delay(50);
  }
  mode = selection;
}

void choosePlayerSymbol() {
  int selection = 1;
  while (true) {
    u8g.firstPage();
    do {
      u8g.setFont(u8g2_font_6x10_tr); u8g.drawStr(0, 10, "Choose your symbol:");
      u8g.setFont(u8g2_font_9x15_tr);
      if (selection == 1) { u8g.drawStr(15, 45, "> O"); u8g.drawStr(65, 45, "  X"); }
      else { u8g.drawStr(15, 45, "  O"); u8g.drawStr(65, 45, "> X"); }
    } while (u8g.nextPage());

    if (digitalRead(BUTTON_MOVE) == LOW) { selection = (selection == 1) ? 2 : 1; while (digitalRead(BUTTON_MOVE) == LOW); delay(200); }
    if (digitalRead(BUTTON_OK) == LOW) { while (digitalRead(BUTTON_OK) == LOW); delay(200); break; }
    delay(50);
  }
  playerSymbol = selection; cpuSymbol = (selection == 1) ? 2 : 1;
}

void boardDrawing() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g2_font_9x15_tr);
    // Kreslenie mriežky
    u8g.drawLine(0, 21, 64, 21); u8g.drawLine(0, 42, 64, 42);
    u8g.drawLine(21, 0, 21, 64); u8g.drawLine(42, 0, 42, 64);
    // Vyplnenie symbolov
    for (int i = 0; i < 9; i++) {
      int x = 5 + 20 * (i % 3); int y = 17 + 20 * (i / 3);
      u8g.drawStr(x, y, charBoard(i));
    }
    showTurnIndicator();
  } while (u8g.nextPage());
}

// Vykreslenie plochy aj s "?" kurzorom pre aktuálny výber hráča
void drawBoardWithCursor(int cursorPos) {
  u8g.firstPage();
  do {
    u8g.setFont(u8g2_font_9x15_tr);
    u8g.drawLine(0, 21, 64, 21); u8g.drawLine(0, 42, 64, 42);
    u8g.drawLine(21, 0, 21, 64); u8g.drawLine(42, 0, 42, 64);
    for (int i = 0; i < 9; i++) {
      int x = 5 + 20 * (i % 3); int y = 17 + 20 * (i / 3);
      u8g.drawStr(x, y, charBoard(i));
    }
    if (board[cursorPos] == 0) {
      int cx = 5 + 20 * (cursorPos % 3); int cy = 17 + 20 * (cursorPos / 3);
      u8g.drawStr(cx, cy, "?");
    }
    showTurnIndicator();
  } while (u8g.nextPage());
}

void showTurnIndicator() {
  u8g.setFont(u8g2_font_9x15_tr);
  if (whosplaying == 1) u8g.drawStr(87, 37, (mode == 0) ? "You" : "P1");
  else u8g.drawStr(87, 37, (mode == 0) ? "CPU" : "P2");
}

void showWinner_TicTacToe(int winnerSymbol) {
  u8g.firstPage();
  do {
    u8g.setFont(u8g2_font_9x15_tr);
    if (winnerSymbol == 3) u8g.drawStr(87, 37, "Draw");
    else {
      if (winnerSymbol == playerSymbol) { u8g.drawStr(87, 37, (mode==0)?"You":"P1"); u8g.drawStr(87, 47, "win"); }
      else { u8g.drawStr(87, 37, (mode==0)?"CPU":"P2"); u8g.drawStr(87, 47, "wins"); }
    }
  } while (u8g.nextPage());
}

bool playAgainMenu_TicTacToe() {
  int selection = 0;
  while (true) {
    u8g.firstPage();
    do {
      u8g.setFont(u8g2_font_6x10_tr); u8g.drawStr(0, 10, "Play again?");
      u8g.setFont(u8g2_font_9x15_tr);
      if (selection == 0) { u8g.drawStr(10, 30, "> Yes"); u8g.drawStr(10, 50, "  Menu"); }
      else { u8g.drawStr(10, 30, "  Yes"); u8g.drawStr(10, 50, "> Menu"); }
    } while (u8g.nextPage());

    if (digitalRead(BUTTON_MOVE) == LOW) { selection = !selection; while (digitalRead(BUTTON_MOVE) == LOW); delay(200); }
    if (digitalRead(BUTTON_OK) == LOW) { while (digitalRead(BUTTON_OK) == LOW); delay(200); break; }
    delay(50);
  }
  return (selection == 0);
}

void playhuman(int symbol) {
  int humanMove = 0; bool stayIn = true; drawBoardWithCursor(humanMove);
  // Cyklus pre výber políčka
  while (stayIn) {
    if (digitalRead(BUTTON_MOVE) == LOW) {
      humanMove = (humanMove + 1) % 9;
      // Preskočenie obsadených políčok
      while (board[humanMove] != 0) humanMove = (humanMove + 1) % 9;
      drawBoardWithCursor(humanMove); while (digitalRead(BUTTON_MOVE) == LOW); delay(150);
    }
    if (digitalRead(BUTTON_OK) == LOW) {
      while (digitalRead(BUTTON_OK) == LOW); delay(150);
      if (board[humanMove] == 0) { board[humanMove] = symbol; stayIn = false; }
    }
  }
}

/*
 * @brief Jednoduchá AI logika
 * 1. Skús vyhrať (nájdi víťazný ťah)
 * 2. Ak nemôžeš vyhrať, zablokuj súpera
 * 3. Ak netreba blokovať, vyber náhodné voľné miesto
 */
void playcpu() {
  int move = findWinningMove(cpuSymbol);
  if (move != -1) { board[move] = cpuSymbol; return; }
  move = findBlockingMove(playerSymbol);
  if (move != -1) { board[move] = cpuSymbol; return; }
  int cpuMove; do { cpuMove = random(0, 9); } while (board[cpuMove] != 0);
  board[cpuMove] = cpuSymbol;
}

int findWinningMove(int symbol) {
  for (int i = 0; i < 9; i++) {
    if (board[i] == 0) {
      board[i] = symbol; checkWinner();
      if (winner == symbol) { board[i] = 0; winner = 0; return i; }
      board[i] = 0; winner = 0;
    }
  } return -1;
}

int findBlockingMove(int opponentSymbol) {
  for (int i = 0; i < 9; i++) {
    if (board[i] == 0) {
      board[i] = opponentSymbol; checkWinner();
      if (winner == opponentSymbol) { board[i] = 0; winner = 0; return i; }
      board[i] = 0; winner = 0;
    }
  } return -1;
}

const char* charBoard(int x) { if (board[x] == 1) return "O"; else if (board[x] == 2) return "X"; else return " "; }

// Kontrola všetkých možných výherných kombinácií (riadky, stĺpce, diagonály)
void checkWinner() {
  for (int i = 1; i <= 2; i++) {
    if ((checkboard(0)==i && checkboard(1)==i && checkboard(2)==i) ||
        (checkboard(3)==i && checkboard(4)==i && checkboard(5)==i) ||
        (checkboard(6)==i && checkboard(7)==i && checkboard(8)==i) ||
        (checkboard(0)==i && checkboard(3)==i && checkboard(6)==i) ||
        (checkboard(1)==i && checkboard(4)==i && checkboard(7)==i) ||
        (checkboard(2)==i && checkboard(5)==i && checkboard(8)==i) ||
        (checkboard(0)==i && checkboard(4)==i && checkboard(8)==i) ||
        (checkboard(2)==i && checkboard(4)==i && checkboard(6)==i)) { winner = i; return; }
  }
  // Kontrola remízy
  bool isDraw = true; for (int i = 0; i < 9; i++) if (board[i] == 0) isDraw = false;
  if (isDraw) winner = 3;
}
int checkboard(int x) { return board[x]; }void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
