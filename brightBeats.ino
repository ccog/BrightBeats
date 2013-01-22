/* File: brightBeats.ino
 * Author: Cam Cogan
 * Desc: Arduino file to record and "play back" drum pattern
 */

#define NUM_DRUMS 4
#define DELAY_FILTER 80
#define MAX_NUM_NOTES 500

/* An array for quick querying of the statuses of the LEDs.
 * 0 -> Snare
 * 1 -> Hi tom
 * 2 -> Lo tom
 * 3 -> Floor tom
 * 4 -> Bass
 */
volatile boolean lightStatusArray[NUM_DRUMS];

//An array for keeping track of the last time each of the drums
//was hit. Follows the same index convention as above
volatile unsigned long timeArray[NUM_DRUMS];

//The array containing the notes played in sequence, along with
//the time since any drum was last hit.
volatile unsigned long noteArray[MAX_NUM_NOTES];

//A counter for navigating the above array and avoiding overflow
volatile unsigned int numNotes;

//A variable for constructing note entries
volatile unsigned long temp;

//An array for use by loop() to turn off LEDs, again same index
//convention.
int ledPins[NUM_DRUMS];

//A counter variable
int i;

//A counter variable for playback
int counter;

//A variable to prevent lots of crowded bit chopping
unsigned long currentWaitTime;

//The digital pin to read in the trigger to switch from recording to playback
int modeSwitch = 13;

//A variable used to avoid several calls to millis() in interrupts
volatile unsigned long currentTime;

//A variable to keep track of time of the last hit
volatile unsigned long lastHitTime;

//Flag to turn off interrupts, as noInterrupts() has been super finnicky
boolean eof;

void setup(){
  noInterrupts();
  eof = false;
  Serial.begin(9600);
  //Initialize the arrays
  for(i = 0; i < NUM_DRUMS; i++){
    lightStatusArray[i] = 0;
    timeArray[i] = 0;
  }
  //Initialize variables integral to recording
  lastHitTime = 0;
  numNotes = 0;
  /* Assigning the LED pins drum-by-drum:
   * Snare -> Pin 4
   * Hi tom -> Pin 5
   * Lo tom -> Pin 6
   * Floor tom -> Pin 7
   * Bass -> Pin 8
   */
  for(i = 0; i < NUM_DRUMS; i++){
    ledPins[i] = 4 + i;
  }
  /* Set pin modes; input pins need to be handled individually because of
   * the screwy external input pin assignments:
   */
  //TODO: SET INPUT PIN MODES HERE
  pinMode(2, INPUT); //Snare
  pinMode(3, INPUT); //Hi tom
  pinMode(19, INPUT); //Lo tom
  pinMode(18, INPUT); //Floor tom
  //Playback switch
  pinMode(modeSwitch, INPUT);
  for(i = 0; i < NUM_DRUMS; i++){
    pinMode(ledPins[i], OUTPUT);
  }
  //Set up interrupts
  attachInterrupt(0, SNARE_isr, RISING); //Snare
  attachInterrupt(1, HI_TOM_isr, RISING); //Hi tom
  attachInterrupt(4, LO_TOM_isr, RISING); //Lo tom
  attachInterrupt(5, FLOOR_TOM_isr, RISING); //Floor tom
  //attachInterrupt(4, BASS_isr, RISING); //Bass
  //attachInterrupt(5, MODE_SWITCH_isr, RISING); //Mode button; yet to implement
  interrupts();
}


/* The only job of the loop function is to ensure that LEDs are
 * switched off after they are switched on in their respective
 * interrupts. It iterates through the status array and, upon 
 * finding an LED that is on, checks to see whether DELAY_FILTER milliseconds
 * have passed since it turned on. If so, it turns off the LED on
 * the board and updates the status array accordingly.
 */
void loop(){
  for(i = 0; i < NUM_DRUMS; i++){
    if(lightStatusArray[i]){
      if(millis() - timeArray[i] > DELAY_FILTER){
        digitalWrite(ledPins[i], LOW);
        lightStatusArray[i] = LOW;
      }
    }
  }
  if(numNotes >= MAX_NUM_NOTES){
    for(i = 0; i < NUM_DRUMS; i++){
      digitalWrite(ledPins[i], LOW);
    }
  }
  //Since the recording algorithm functions primarily via interrupts, the
  //check for the playback switch can be accomplished via polling the corresponding
  //digital pin.
  if(digitalRead(modeSwitch)){
    playBack();
  }
  Serial.println(numNotes);
}

void playBack(){
  const int limit = (int) numNotes;
  eof = true;
  //First, ensure all LEDs are off for proper playback
  for(i = 0; i < NUM_DRUMS; i++){
    digitalWrite(ledPins[i], LOW);
    lightStatusArray[i] = false;
  }
  //Delay 1 second...
  delay(1000);
  counter = 0;
  while(counter < limit){
    if(!counter){
      lastHitTime = millis();
      currentWaitTime = 0;
    }
    //If wait until next hit has elapsed, write necessary LED high, update
    //the last hit time, update the LED's entry in the time array, increment
    //to the next note in the list, and update the wait time
    if(millis() - lastHitTime >= currentWaitTime){
      digitalWrite(ledPins[((int) ((noteArray[counter]) >> 29))], HIGH);
      lightStatusArray[((int) ((noteArray[counter]) >> 29))] = true;
      lastHitTime = millis();
      timeArray[((int) ((noteArray[counter]) >> 29))] = lastHitTime;
      counter++;
      currentWaitTime = (noteArray[counter]) & 0x1FFFFFFF;
    }
    //Iterate through LEDs and see if any need be turned off
    for(i = 0; i < NUM_DRUMS; i++){
      if(lightStatusArray[i]){
        if(millis() - timeArray[i] > DELAY_FILTER){
          digitalWrite(ledPins[i], LOW);
          lightStatusArray[i] = false;
        }
      }
    }
    //Serial.println(numNotes);    
  }
  delay(DELAY_FILTER);
  //Signal pattern is over
  for(i = 0; i < NUM_DRUMS; i++){
    digitalWrite(ledPins[i], LOW);
  }/*
  delay(200);
  for(i = 0; i < NUM_DRUMS; i++){
    digitalWrite(ledPins[i], LOW);
  }*/
  //Serial.println("Made it!");
  while(1);
}

void SNARE_isr(){
  currentTime = millis();
  if(currentTime - timeArray[0] > DELAY_FILTER && numNotes < MAX_NUM_NOTES && !eof){
    //If this is the first hit, we want the time since last hit to be 0. Since
    //time doesn't increment in the interrupt, we can ensure this with the following:
    if(!numNotes){
      lastHitTime = currentTime;
    }
    temp = currentTime - lastHitTime;
    //Set first three bits to 000
    temp = temp & 0x1FFFFFFF;
    noteArray[numNotes] = temp;
    lastHitTime = currentTime;
    timeArray[0] = currentTime;
    digitalWrite(ledPins[0], HIGH);
    lightStatusArray[0] = HIGH;
    numNotes++;
  }
}

void HI_TOM_isr(){
  currentTime = millis();
  if(currentTime - timeArray[1] > DELAY_FILTER && numNotes < MAX_NUM_NOTES && !eof){
    if(!numNotes){
      lastHitTime = currentTime;
    }
    temp = currentTime - lastHitTime;
    //Set first three bits to 001
    temp = temp & 0x3FFFFFFF;
    temp = temp | 0x20000000;
    noteArray[numNotes] = temp;
    lastHitTime = currentTime;
    timeArray[1] = currentTime;
    digitalWrite(ledPins[1], HIGH);
    lightStatusArray[1] = HIGH;
    numNotes++;
  }
}

void LO_TOM_isr(){
  currentTime = millis();
  if(currentTime - timeArray[2] > DELAY_FILTER && numNotes < MAX_NUM_NOTES && !eof){
    if(!numNotes){
      lastHitTime = currentTime;
    }
    temp = currentTime - lastHitTime;
    //Set first three bits to 010
    temp = temp & 0x5FFFFFFF;
    temp = temp | 0x40000000;
    noteArray[numNotes] = temp;
    lastHitTime = currentTime;
    timeArray[2] = currentTime;
    digitalWrite(ledPins[2], HIGH);
    lightStatusArray[2] = HIGH;
    numNotes++;
  }
}

void FLOOR_TOM_isr(){
  currentTime = millis();
  if(currentTime - timeArray[3] > DELAY_FILTER && numNotes < MAX_NUM_NOTES && !eof){
    if(!numNotes){
      lastHitTime = currentTime;
    }
    temp = currentTime - lastHitTime;
    //Set first three bits to 011
    temp = temp & 0x7FFFFFFF;
    temp = temp | 0x60000000;
    noteArray[numNotes] = temp;
    lastHitTime = currentTime;
    timeArray[3] = currentTime;
    digitalWrite(ledPins[3], HIGH);
    lightStatusArray[3] = HIGH;
    numNotes++;
  }
}
/*
void BASS_isr(){
  currentTime = millis();
  if(currentTime - timeArray[4] > DELAY_FILTER numNotes < MAX_NUM_NOTES){
    if(!numNotes){
      lastHitTime = currentTime;
    }
    temp = currentTime - lastHitTime;
    //Set first three bits to 100
    temp = temp & 0x9FFFFFFF;
    temp = temp | 0x80000000;
    noteArray[numNotes] = temp;
    lastHitTime = currentTime;
    timeArray[4] = currentTime;
    digitalWrite(ledPins[4], HIGH);
    lightStatusArray[4] = HIGH;
    numNotes++;
  }
}*/
