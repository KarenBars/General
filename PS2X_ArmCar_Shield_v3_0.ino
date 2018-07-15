#include "PS2X_lib.h"  //for v1.6
#include <Wire.h>
#include "Adafruit_MotorShield.h"
#include "Adafruit_MS_PWMServoDriver.h"
#include <Adafruit_PWMServoDriver.h>

/******************************************************************
 * set pins connected to PS2 controller:
 *   - 1e column: original 
 *   - 2e colmun: Stef?
 * replace pin numbers by the ones you use
 ******************************************************************/
#define PS2_DAT        12  
#define PS2_CMD        11  
#define PS2_SEL        10 
#define PS2_CLK        13 

/******************************************************************
 * select modes of PS2 controller:
 *   - pressures = analog reading of push-butttons 
 *   - rumble    = motor rumbling
 * uncomment 1 of the lines for each mode selection
 ******************************************************************/
#define pressures   true 
#define rumble      true

PS2X ps2x; // create PS2 Controller Class

int error = 0;
byte type = 0;
byte vibrate = 0;

int lx = 0;
int ly = 0;
int L_Speed = 0;
int R_Speed = 0;

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
// Or, create it with a different I2C address (say for stacking)
// Adafruit_MotorShield AFMS = Adafruit_MotorShield(0x61); 

// Select which 'port' M1, M2, M3 or M4. In this case, M1
Adafruit_DCMotor *L_M3 = AFMS.getMotor(3);
Adafruit_DCMotor *R_M4 = AFMS.getMotor(4);
// You can also make another motor on port M2
//Adafruit_DCMotor *myOtherMotor = AFMS.getMotor(2);

// Servos on PCA9685
Adafruit_PWMServoDriver PWM = Adafruit_PWMServoDriver(0x40);
// #define MIN_PULSE_WIDTH       650
// #define MAX_PULSE_WIDTH       2350
// #define DEFAULT_PULSE_WIDTH   1500 
// #define FREQUENCY             50
// uint8_t servonum = 0;

boolean PSB_TRIANGLE_flag = 0;
boolean PSB_TRIANGLE_butt;
boolean led_flag = 0;
unsigned long last_press;

const int SERVOS = 4;
const int HeadL_LED = 7;
const int Stop_LED = 8;
const int ACC = 10; // the accurancy of the potentiometer value before idle starts counting
int PIN[SERVOS], value[SERVOS], idle[SERVOS], currentAngle[SERVOS], MIN[SERVOS], MAX[SERVOS], INITANGLE[SERVOS], previousAngle[SERVOS],ANA[SERVOS];

void setServoPulse(uint8_t n, double pulse) {
  double pulselength;
  pulselength = 1000000;   // 1,000,000 us per second
  pulselength /= 60;   // 60 Hz
  Serial.print(pulselength); Serial.println(" us per period"); 
  pulselength /= 4096;  // 12 bits of resolution
  Serial.print(pulselength); Serial.println(" us per bit"); 
  pulse *= 1000;
  pulse /= pulselength;
  Serial.println(pulse);
  AFMS.setPWM(n, pulse);
}

// Angle to PWM
  void writeServo(uint8_t n,uint8_t angle){
  double pulse;
  pulse=0.5+angle/90.0;
  setServoPulse(n,pulse);
}

void setup(){
  Serial.begin(57600);
  delay(2000);  //added delay to give wireless ps2 module some time to startup, before configuring it
  Serial.print("Search Controller..");
  //CHANGES for v1.6 HERE!!! **************PAY ATTENTION*************
  do{
  //setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
    error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
    if(error == 0){
      Serial.println("\nConfigured successful ");
      break;
    }else{
      Serial.print(".");
      delay(100);
    }
  }while(1);  
  type = ps2x.readType(); 
  switch(type) {
    case 0:
      Serial.println("Unknown Controller type found ");
      break;
    case 1:
      Serial.println("DualShock Controller found ");
      break;
    case 2:
      Serial.println("GuitarHero Controller found ");
      break;
    case 3:
      Serial.println("Wireless Sony DualShock Controller found ");
      break;
   }
   ps2x.read_gamepad(true, 200);
   delay(500);
   ps2x.read_gamepad(false, 200);
   delay(300);
   ps2x.read_gamepad(true, 200);
   delay(500);

  PWM.begin();
  PWM.setPWMFreq(200);

    // Base/arm rotation Servo
  PIN[0] = 0;
  MIN[0] = 0;
  MAX[0] = 180;
  INITANGLE[0] = 90;
  ANA[0] = 3;
  // First Level Servo
  PIN[1] = 1;
  MIN[1] = 0; 
  MAX[1] = 180;
  INITANGLE[1] = 5;
  ANA[1] = 2;
  // Second Level Servo
  PIN[2] = 14;
  MIN[2] = 0;
  MAX[2] = 180;
  INITANGLE[2] = 170;
  ANA[2] = 2;
  // Third Level Servo
  PIN[3] = 15;
  MIN[3] = 0;
  MAX[3] = 180;
  INITANGLE[3] = 90;
  ANA[3] = 2;
  
  AFMS.begin(50);  // create with the default frequency 1.6KHz
  for (int i = 0; i < SERVOS; i++){
    value[i] = 0;
    idle[i] = 0;
    previousAngle[i]=INITANGLE[i];
    currentAngle[i]=INITANGLE[i];
    writeServo(PIN[i],INITANGLE[i]);
  }
  {
  // initialize the LED pins as outputs (for head and stop lights):
  pinMode(HeadL_LED, OUTPUT);
  pinMode(Stop_LED, OUTPUT);
  }
}

void loop() {
  /* You must Read Gamepad to get new values and set vibration values
     ps2x.read_gamepad(small motor on/off, larger motor strenght from 0-255)
     if you don't enable the rumble, use ps2x.read_gamepad(); with no values
     You should call this at least once a second
   */  
    ps2x.read_gamepad(false, vibrate); //read controller and set large motor to spin at 'vibrate' speed
    
    if(ps2x.Button(PSB_START)){         //will be TRUE as long as button is pressed
      Serial.println("Start is being held");
      ps2x.read_gamepad(true, 200);
    }
    if(ps2x.Button(PSB_SELECT)){
      Serial.println("Select is being held");      
    }
    if(ps2x.Button(PSB_PAD_UP)) {      //will be TRUE as long as button is pressed
      Serial.print("UP held this hard: ");
      Serial.println(ps2x.Analog(PSAB_PAD_UP), DEC);
      digitalWrite (Stop_LED, 0);
      L_M3->run(FORWARD);
      R_M4->run(FORWARD);
      L_Speed = ps2x.Analog(PSAB_PAD_UP);
      R_Speed = ps2x.Analog(PSAB_PAD_UP);  
    }else if(ps2x.ButtonReleased(PSB_PAD_UP))  {
      digitalWrite (Stop_LED, 1);
      L_M3->run(RELEASE);
      R_M4->run(RELEASE);      
    }
    if(ps2x.Button(PSB_PAD_DOWN)){
      Serial.print("DOWN held this hard: ");
      Serial.println(ps2x.Analog(PSAB_PAD_DOWN), DEC);
      L_Speed = ps2x.Analog(PSAB_PAD_DOWN);
      R_Speed = ps2x.Analog(PSAB_PAD_DOWN);
      digitalWrite (Stop_LED, 0);   
      L_M3->run(BACKWARD);
      R_M4->run(BACKWARD);
    }else if(ps2x.ButtonReleased(PSB_PAD_DOWN))  {
      digitalWrite (Stop_LED, 1);
      L_M3->run(RELEASE);
      R_M4->run(RELEASE);      
    }
    if(ps2x.Button(PSB_R1)){
      R_Speed = 0;
    }
    if(ps2x.Button(PSB_L1)){
      L_Speed = 0;
    }
    L_M3->setSpeed(L_Speed);
    R_M4->setSpeed(R_Speed);
    
    if(ps2x.Button(PSB_PAD_LEFT)){
      Serial.print("LEFT held this hard: ");
      Serial.println(ps2x.Analog(PSAB_PAD_LEFT), DEC);
      L_Speed = ps2x.Analog(PSAB_PAD_LEFT);
      R_Speed = ps2x.Analog(PSAB_PAD_LEFT);
      L_M3->run(BACKWARD);
      R_M4->run(FORWARD);
      L_M3->setSpeed(250);
      R_M4->setSpeed(250);
    }else if(ps2x.ButtonReleased(PSB_PAD_LEFT))  {
      L_M3->run(RELEASE);
      R_M4->run(RELEASE);
    }
    if(ps2x.Button(PSB_PAD_RIGHT)){
      Serial.print("RIGHT held this hard: ");
      Serial.println(ps2x.Analog(PSAB_PAD_RIGHT), DEC);
      L_Speed = ps2x.Analog(PSAB_PAD_RIGHT);
      R_Speed = ps2x.Analog(PSAB_PAD_RIGHT);
//    Serial.println(ps2x.Analog(PSB_PAD_RIGHT));
      L_M3->run(FORWARD);
      R_M4->run(BACKWARD);
      L_M3->setSpeed(250);
      R_M4->setSpeed(250);
    }else if(ps2x.ButtonReleased(PSB_PAD_RIGHT))  {
      L_M3->run(RELEASE);
      R_M4->run(RELEASE);      
    }
    
    vibrate = ps2x.Analog(PSAB_CROSS);  //this will set the large motor vibrate speed based on how hard you press the blue (X) button
    if (ps2x.NewButtonState()) {        //will be TRUE if any button changes state (on to off, or off to on)
      if(ps2x.Button(PSB_R1))
        Serial.println("R1 pressed");
      if(ps2x.Button(PSB_R2))
        Serial.println("R2 pressed");
    }
    if(ps2x.ButtonPressed(PSB_L1)){               //will be TRUE if button was JUST pressed
      Serial.println("L1 pressed - Open Gripper");
      PWM.setPWM(14, 5, 100);
    }
    if(ps2x.ButtonPressed(PSB_L2)){               //will be TRUE if button was JUST pressed
      Serial.println("L2 pressed - Close Gripper");
      PWM.setPWM(14, 100, 5);
    }
 {
  PSB_TRIANGLE_butt = ps2x.Button(PSB_TRIANGLE); // считать текущее положение PSB_TRIANGLE
  
  if (PSB_TRIANGLE_butt == 1 && PSB_TRIANGLE_flag == 0 && millis() - last_press > 100) {
    PSB_TRIANGLE_flag = 1;
    Serial.println("Head Lights ON");
    led_flag = !led_flag;
    digitalWrite(HeadL_LED, led_flag);
    last_press = millis();
     }
  if (PSB_TRIANGLE_butt == 0 && PSB_TRIANGLE_flag == 1) {
    PSB_TRIANGLE_flag = 0;
    Serial.println("Head Lights OFF");
      }
  }
   
    if(ps2x.ButtonPressed(PSB_CIRCLE)){               //will be TRUE if button was JUST pressed
      Serial.println("Circle just pressed");
    }
    if(ps2x.NewButtonState(PSB_CROSS)) {              //will be TRUE if button was JUST pressed OR released
      Serial.println("X just changed");
      ps2x.read_gamepad(true, vibrate);   
  }
    if(ps2x.ButtonPressed(PSB_SQUARE)) {             //will be TRUE if button was JUST released
      Serial.println("Square just released");
    }
    if(ps2x.Button(PSB_L3) || ps2x.Button(PSB_R3)) { //print stick values if either is TRUE
      Serial.print("Stick Values:");
      Serial.print(ps2x.Analog(PSS_LY), DEC); //Left stick, Y axis. Other options: LX, RY, RX  
      Serial.print(",");
      Serial.print(ps2x.Analog(PSS_LX), DEC); 
      Serial.print(",");
      Serial.print(ps2x.Analog(PSS_RY), DEC); 
      Serial.print(",");
      Serial.println(ps2x.Analog(PSS_RX), DEC); 
    }       
  
  value[0] = ps2x.Analog(PSS_LX);
  value[1] = ps2x.Analog(PSS_RY);
  value[2] = ps2x.Analog(PSS_LY);
  value[3] = ps2x.Analog(PSS_RX);
  
   for (int i = 0; i < SERVOS; i++){
    if (value[i] > 130) {
      if (currentAngle[i] < MAX[i]) currentAngle[i]+=1;
      writeServo(PIN[i],currentAngle[i]);     
    } else if (value[i] < 120) {
      if (currentAngle[i] > MIN[i]) currentAngle[i]-=1;
      writeServo(PIN[i],currentAngle[i]);    
    }  
  }  
  
  delay(10);
}
