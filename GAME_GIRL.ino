
// GAMEGIRL
// Author: Saifeddine ALOUI AKA SNetuino
// With the help from Line ALOUI

// Import libraries
#include <RTTTL.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ===========================
// Defines
// ===========================
// prepare software
#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args)  write(args);
#else
#define printByte(args)  print(args,BYTE);
#endif
#define NB_MAX_HIGH_SCORES 5


// ===========================
// Enums
// ===========================

// Game parameters
typedef enum{
  status_menu,
  status_playing,
  status_gameover
}System_Status;


// ===========================
// Structures
// ===========================
struct Game{ // Every game has an instance from this structure that defines the game name and the function to call to launch the game
  char* name;
  void (*fn)(void);
};

struct ScoreInfo { // Score information (each highscore is composed of the value of the score and the name (3 chars + 0)
  int score;
  char name[4];
};

struct HighScores { // This structure will be flashed to the EEPROM to save high scores
  int hash;
  ScoreInfo scores[NB_MAX_HIGH_SCORES]; //
};

// ===========================
// global variables
// ===========================
// *** High scores ***
int eeAddress=0; // Address to put highscore data on the EEPROM
HighScores highScores; // highscores structure to save on the EEPROM

// *** Games definition ***
// Games functions prototypes
void reflex_tester(); // A game to test reflexes
void color_memory(); // A game to test capacity to memorize colors sequences
void display_highscores(); // A code to show the savced highscores

Game games[]={ // Here we define games. Add your own games  
  {.name="Reaction test", .fn=reflex_tester},  
  {.name="Color memory", .fn=color_memory},
  {.name="High scores", .fn=display_highscores}
};
int nb_games=sizeof(games)/sizeof(Game); // Computes the number of games
int game_id=0; // Currently selected game index
int high_score_id=0; // Current high score index

// *** Global gaming status ***
System_Status system_status = status_menu; // 0 => Menu, 1=> Playing, 2=> Game over

// *** Hardware variables ***
// Prepare liquid crystal
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display_infos
// Define a heart needed to show how many lives left
uint8_t heart[8] = {0x0,0xa,0x1f,0x1f,0xe,0x4,0x0};

// -- Pins --
// Buttons and leds connected on D2 to D9 button led, button led...
int btn_red = 2;
int led_red = 3;
int btn_yellow = 4;
int led_yellow = 5;
int btn_green = 6;
int led_green = 7;
int btn_blue = 8;
int led_blue = 9;

// An array of buttons and leds (useful to games that use color indexing)
int btns_list[]={btn_red, btn_yellow, btn_green, btn_blue};
int leds_list[]={led_red, led_yellow, led_green, led_blue};

// -- Buzzer --
int audio_out=11;


// *** Audio music ***
// Instantiate the RTTTL object
RTTTL rtttl = RTTTL(audio_out);

// -- Music and sound effects ---
char intro[]="smb:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6";
char Ready[]="rd:d=4,o=5,b=355:16a6"; // Ready
// Buttons sound effects 
char m_red[]="rd:d=4,o=5,b=355:16c6"; 
char m_yellow[]="rd:d=4,o=5,b=355:16d6";
char m_green[]="rd:d=4,o=5,b=355:16e6";
char m_blue[]="rd:d=4,o=5,b=355:16f6";

// Sound effefcts for good and bad responses
char OK[]="smb:d=4,o=5,b=100:16c6,16a6";
char KO[]="smb:d=4,o=5,b=100:16a6,16c6";

// Game over music
char m_game_over[]="gameover:d=4,o=4,b=200:8c5,4p,8g4,4p,4e4,6a4,6b4,6a4,6g#4,6a#4,6g#4,4g4,4f4,2g4";

// List of button sound effectus to simplify indexing
char * audio_list[]={m_red, m_yellow, m_green, m_blue};


// *** Gaming information ***
int level=0; // Current level
int score=0; // Current score
int lives=3; // Number of lives 


// *** Specific games variables ***
// Reflex tester params
int current_color=0;

// Color memory params
int mem_buffer[10];  // A buffer containing the sequence of colors to do
int memory_status=0; // Current position of the pressed 


// ===========================
// Helper functions
// ===========================
// -- EEPROM read/write --
// Loads scores from EEPROM
void load_scores()
{
  EEPROM.get(eeAddress, highScores);  
  if(highScores.hash!=0x5A1F)
  {
    for (int i=0;i<5;i++)
    {
      highScores.scores[i].score=0;
      highScores.scores[i].name[0]=0;
      highScores.scores[i].name[1]=0;
      highScores.scores[i].name[2]=0;
      highScores.scores[i].name[3]=0;
    }
    highScores.hash=0x5A1F;
    EEPROM.put(eeAddress, highScores); 
  }
}

// -- LED --
// Drive leds using software PWM
// param pin  : Led pin
// param ampl : Amplitude of light (0 -> 255)
// duration : This function blocks for duration milliseconds
void soft_pwm(int pin, int ampl, int duration)
{
  for (int i=0;i<duration; i++)
  {
    digitalWrite(pin, HIGH);
    delayMicroseconds(ampl); // Approximately 10% duty cycle @ 1KHz
    digitalWrite(pin, LOW);
    delayMicroseconds(1000 - ampl);
  }
}

// Drive leds using software PWM while playing an audio
// param pin  : Led pin
// param ampl : Amplitude of light (0 -> 255)
// max_duration : This function blocks for duration milliseconds at max. If audio finishes first, this function returns
// audio : The audio to be played (RTTTL string)
//
void soft_pwm_withaudio(int pin, int ampl, int max_duration, char *audio)
{
  rtttl.playMelody(audio);
  int i=0;
  while(i< max_duration && max_duration>0 || rtttl.m_currentNote.info != RTTTL_INFO_EOM)
  {
    digitalWrite(pin, HIGH);
    delayMicroseconds(ampl); // Approximately 10% duty cycle @ 1KHz
    digitalWrite(pin, LOW);
    delayMicroseconds(1000 - ampl);
    rtttl.tick();
    i++;
  }
}

void reset()
{
   level=0;
   score=0;
   lives=3;
}

// -- Main menu --
// Updates main menu interface
void update_menu()
{
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(games[game_id].name);
  if(game_id+1<nb_games)
  {
    lcd.setCursor(1, 1);
    lcd.print(games[game_id+1].name);
  }
  lcd.setCursor(0, 0);  
  lcd.print(">");  
}
// Displays and manages main menu and games launching system
void display_menu()
{
  int validated=false;
  update_menu();
    
  while(!validated)
  {
    if(!digitalRead(btn_blue))
    {
      game_id-=1;
      if(game_id==-1)
      {
        game_id=nb_games-1;
      }
      update_menu();
      soft_pwm_withaudio(led_blue, 50,200, m_blue);
    }
    if(!digitalRead(btn_green))
    {
      game_id+=1;
      if(game_id==nb_games)
      {
        game_id=0;
      }
      update_menu();
      
      soft_pwm_withaudio(led_green, 50, 200, m_green);
    }
    if(!digitalRead(btn_yellow))
    {
      validated=true;
      system_status = status_playing;
      reset();
      update_menu();
      soft_pwm_withaudio(led_yellow,100, 200, m_yellow);
      display_infos();
      delay(1000);
    }
  }
}
// -- HighScores --
// Updates highscore LCD UI
void update_highscore()
{
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(high_score_id);
  lcd.print(" : ");  
  lcd.print(highScores.scores[high_score_id].name);
  lcd.print(" : ");  
  lcd.print(highScores.scores[high_score_id].score);
  if(high_score_id+1<NB_MAX_HIGH_SCORES)
  { 
    lcd.setCursor(1, 1);
    lcd.print(high_score_id+1);
    lcd.print(" : ");  
    lcd.print(highScores.scores[high_score_id+1].name);
    lcd.print(" : ");  
    lcd.print(highScores.scores[high_score_id+1].score);
  }
  lcd.setCursor(0, 0);  
}
// Lisplay and manage highscores list
void display_highscores()
{
  int validated=false;
  high_score_id =0;
  update_highscore();
  
  while(!validated)
  {
    if(!digitalRead(btn_blue))
    {
      high_score_id-=1;
      if(high_score_id==-1)
      {
        high_score_id=NB_MAX_HIGH_SCORES-1;
      }
      update_highscore();
      soft_pwm_withaudio(led_blue, 50,200, m_blue);
    }
    if(!digitalRead(btn_green))
    {
      high_score_id+=1;
      if(high_score_id==NB_MAX_HIGH_SCORES)
      {
        high_score_id=0;
      }
      update_highscore();
      
      soft_pwm_withaudio(led_green, 50, 200, m_green);
    }
    if(!digitalRead(btn_yellow))
    {
      validated=true;
      system_status = status_menu;
      reset();
      update_highscore();
      soft_pwm_withaudio(led_yellow,100, 200, m_yellow);
      update_highscore();
      delay(1000);
    }
  }
}

// -- Gameover --
// Show current highscore
void show_high_score(int id, int current_idx)
{
    lcd.clear();
    lcd.print("High score :"); 
    lcd.print(id+1);          
    lcd.setCursor(4, 1);
    lcd.print(":"); 
    lcd.print(score);          
    for(int char_id=0;char_id<=current_idx;char_id++)
    {
      lcd.setCursor(char_id, 1);
      lcd.print(highScores.scores[id].name[char_id]);
    }
}
// Galme is over UI
void game_over()
{
    bool get_out=false;
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("GAME OVER"); 
    lcd.setCursor(0, 1);
    lcd.print("score :"); 
    lcd.print(score); 
    play_melody_with_leds(50, m_game_over);
    // show scores
    for (int i=0;i<NB_MAX_HIGH_SCORES;i++)
    {
      if(score>highScores.scores[i].score)
      {
        for(int j=4;j>=i+1;j--)
        {
          highScores.scores[j].score=highScores.scores[j-1].score;
          highScores.scores[j].name[0]=highScores.scores[j-1].name[0];
          highScores.scores[j].name[1]=highScores.scores[j-1].name[1];
          highScores.scores[j].name[2]=highScores.scores[j-1].name[2];
        }

        
        highScores.scores[i].score = score;
        get_out=false;
        int char_id=0;
        highScores.scores[i].name[char_id]='A';
        show_high_score(i , char_id);        
        while(!get_out)
        {
          if(!digitalRead(btn_blue))
          {
            highScores.scores[i].name[char_id] -=1;
            if(highScores.scores[i].name[char_id]<'A')
              highScores.scores[i].name[char_id]='Z';
            show_high_score(i , char_id);        
            soft_pwm_withaudio(led_blue, 50,200, m_blue);       
          }
          if(!digitalRead(btn_green))
          {
            highScores.scores[i].name[char_id] +=1;
            if(highScores.scores[i].name[char_id]>'Z')
              highScores.scores[i].name[char_id]='A';
            show_high_score(i , char_id);        
            soft_pwm_withaudio(led_green, 50,200, m_green);              
          }
          
          if(!digitalRead(btn_yellow))
          {
            char_id ++;
            if(char_id==3)
            {
              get_out=true;
              highScores.scores[i].name[char_id]=0;
              EEPROM.put(eeAddress, highScores); 
            }
            else
            {
              highScores.scores[i].name[char_id]='A';
            }
            show_high_score(i , char_id);        
            soft_pwm_withaudio(led_yellow, 50,200, m_yellow); 
          }
                    
        }
        break;
      }
    }
    // wait for choice
    get_out=false;
    while(!get_out)
    {
      if(!digitalRead(btn_yellow))
      {
        get_out=true;
        system_status = status_menu;
        delay(200);
      }
      if(!digitalRead(btn_red))
      {
        get_out=true;
        reset();
        system_status=status_playing;
        display_infos();
        delay(200);
      }
    }    
}

// -- Useful UI --
// Display useful information (Score + Lives)
void display_infos()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("score :"); 
    lcd.print(score); 
    lcd.setCursor(0, 1);
    lcd.print("Lives :");
    for (int i=0;i<lives;i++)
      lcd.printByte(3); 
}
// Display OK
void display_ok()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(":)"); 
    lcd.setCursor(0, 1);
    lcd.print("O.K."); 
}
// Display KO
void display_ko()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(":("); 
    lcd.setCursor(0, 1);
    lcd.print("K.O."); 
}
// Displays level up interface
void display_LEVEL_UP()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Level Up"); 
    lcd.setCursor(0, 1);
    lcd.print(level); 
    delay(1000);
}

// -- Audio play --
// Plays a melody while randomly showing leds
void play_melody_with_leds(int led_ampl, char* melody)
{
  rtttl.playMelody(melody, false);
  int led_id=random(4);
  int hold=false;
  int current_note = 0;
  while( rtttl.m_currentNote.info != RTTTL_INFO_EOM )
  {
    rtttl.tick();
    if(!hold)
      digitalWrite(leds_list[led_id], HIGH);
    delayMicroseconds(led_ampl); // Approximately 10% duty cycle @ 1KHz
    if(!hold)
      digitalWrite(leds_list[led_id], LOW);
    delayMicroseconds(1000 - led_ampl);
    if(current_note != rtttl.m_currentNote.note)
    {
      if(rtttl.m_currentNote.note>0)
      {
        led_id=random(4);
        hold=false;
      }
      else
      {
        hold=true;        
      }
      current_note = rtttl.m_currentNote.note;
    }
  }
}




// ===========================
// Arduino setup functions
// ===========================
// Prepare game
void setup() {
  // put your setup code here, to run once:
  pinMode(audio_out, OUTPUT);   // sets the pin as output
  Serial.begin(115200);
  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  pinMode(btn_red, INPUT_PULLUP); 
  pinMode(led_red, OUTPUT); 
  pinMode(btn_yellow, INPUT_PULLUP); 
  pinMode(led_yellow, OUTPUT); 
  pinMode(btn_green, INPUT_PULLUP); 
  pinMode(led_green, OUTPUT); 
  pinMode(btn_blue, INPUT_PULLUP); 
  pinMode(led_blue, OUTPUT); 
  lcd.home();
  lcd.createChar(3, heart);
  randomSeed(analogRead(0));
  
  // Show interface
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("GAME GIRL");
  play_melody_with_leds(50, intro);


  // Load scores
  load_scores();
  
  // Game menu
  display_menu();
  
}

// ================================
// Main Arduino loop
// ================================
void loop()
{
int state = 0; // 0 => Menu, 1=> Playing, 2=> Game over
  switch(system_status)
  {
    case status_menu:
      display_menu();
      break;
    
    case status_playing:
      games[game_id].fn();
      break;

    case status_gameover:
      game_over();
      break;
  }
}


// ================================
// Games
// ================================

// ============================ Reflex tester =========================
void show_led_and_wait_reaction(int led_id, int ampl, int max_duration)
{
  bool is_out=0;
  rtttl.playMelody(audio_list[led_id]);
  int i=0;
  display_infos();
  while((i< max_duration && max_duration>0 || rtttl.m_currentNote.info != RTTTL_INFO_EOM ) && !is_out)
  {
    digitalWrite(leds_list[led_id], HIGH);
    delayMicroseconds(ampl); // Approximately 10% duty cycle @ 1KHz
    digitalWrite(leds_list[led_id], LOW);
    delayMicroseconds(1000 - ampl);
    rtttl.tick();
    i++;

    for(int j=0;j<4;j++)
    {
      if(!digitalRead(btns_list[j]))
      {
        if(led_id==j)
        {
          score+=1;
          Serial.println("DATA:4");
          soft_pwm_withaudio(leds_list[j],ampl,0, OK);
        }
        else
        {
          lives--;
          soft_pwm_withaudio(leds_list[j],ampl,0, KO);
        }
        display_infos();
        is_out=true;
      }
    }
  }
  if(i>= max_duration && max_duration>0)
  {
        lives--;
        soft_pwm_withaudio(leds_list[i],ampl,0, KO);
  }

}

void reflex_tester() {
  current_color=random(4);  
  show_led_and_wait_reaction(current_color,50,1000);
  if(lives==0)
  {
    system_status=status_gameover;
  }
}




// ========================= Color MEMORY +++++++++++
// Generate colors sequence
void generate_sequence()
{
  for(int i=0;i<level+3;i++)
    mem_buffer[i]=random(4)+1;
}
// Main game function for Color Memory
void color_memory() {
  switch(memory_status)
  {
    case 0:
      generate_sequence();
      memory_status=1;
      break;
    case 1: // Affichage des couleurs
      for (int i=0;i<3+level;i++)
      {
        switch(mem_buffer[i])
        {
          case 1:
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("Red");
            soft_pwm_withaudio(led_red,100,1000, m_red);
            break;
          case 2:
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("Yellow");
            soft_pwm_withaudio(led_yellow,100,1000, m_yellow);
            break;
          case 3:
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("Green");
            soft_pwm_withaudio(led_green, 50,1000, m_green);
            break;
          case 4:
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("Blue");
            soft_pwm_withaudio(led_blue, 50,1000, m_blue);
            break;
        }
      }
      current_color = 0;
      memory_status=2;
      display_infos();
      break;
    case 2: // Attente de réaction utilisateur
        if(!digitalRead(btn_red))
        {
          if(mem_buffer[current_color]==1)
          {
            display_ok();
            current_color ++;
            Serial.println("DATA:1");
            soft_pwm_withaudio(led_red,100,0, OK);
          }
          else
          {
            display_ko();
            lives--;
            rtttl.playMelody(KO,false, true);
            memory_status=0;
          }
          display_infos();
        }
        if(!digitalRead(btn_yellow))
        {
          if(mem_buffer[current_color]==2)
          {
            display_ok();
            current_color ++;
            Serial.println("DATA:2");
            soft_pwm_withaudio(led_yellow,100,0, OK);
          }
          else
          {
            display_ko();
            lives--;
            rtttl.playMelody(KO,false, true);
            memory_status=0;
          }
          display_infos();
        }    
        if(!digitalRead(btn_green))
        {
          if(mem_buffer[current_color]==3)
          {
            display_ok();
            current_color ++;
            Serial.println("DATA:3");
            soft_pwm_withaudio(led_green, 50,0, OK);
          }
          else
          {
            display_ko();
            lives--;
            rtttl.playMelody(KO,false, true);
            memory_status=0;
          }
          display_infos();
        }
        if(!digitalRead(btn_blue))
        {
          if(mem_buffer[current_color]==4)
          {
            display_ok();
            current_color ++;
            Serial.println("DATA:4");
            soft_pwm_withaudio(led_blue, 50,0, OK);
          }
          else
          {
            display_ko();
            lives--;
            rtttl.playMelody(KO,false, true);
            memory_status=0;
          }
          display_infos();
        }
        if(current_color==level+3)
        {
          score ++;
          if(score%5==0)
          {
            level++;
            display_LEVEL_UP();
          }
          memory_status=0;
          display_infos();
        }
         
        break;
  }
  if(lives==0)
  {
    system_status=status_gameover;
  }
}
