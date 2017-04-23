// Record sound as raw data to a SD card, and play it back.
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// Three pushbuttons need to be connected:
//   Record Button: pin 0 to GND
//   Stop Button:   pin 1 to GND
//   Play Button:   pin 2 to GND
//
// This example code is in the public domain.

#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#define AUDIOSHIELD 1

// 0 -record, 1 - play
#define MMODE  1

// GUItool: begin automatically generated code
AudioPlaySdRaw           playRaw1;       //xy=302,157
AudioRecordQueue         queue1;         //xy=281,63
AudioRecordQueue         queue2;         //xy=281,63
#ifdef AUDIOSHIELD
  AudioInputI2S      adc1;
  AudioOutputI2S     i2s1;           //xy=470,120
  AudioControlSGTL5000     sgtl5000_1;     //xy=265,212
  AudioConnection          patchCord_LO(playRaw1, 0, i2s1, 0);
  AudioConnection          patchCord_RO(playRaw1, 0, i2s1, 1);
  AudioConnection          patchCord1(adc1, 1, queue2, 0);
#else
  //AudioInputAnalogStereo   adc1;          
  AudioInputAnalog         adc1(A3);  
  AudioOutputAnalogStereo  dacs1;    
  AudioConnection          patchCord3(playRaw1, 0, dacs1, 0);    
#endif  
AudioConnection          patchCord1(adc1, 0, queue1, 0);


// GUItool: end automatically generated code

#define BUTT_RECORD 37
#define BUTT_STOP 38
#define BUTT_PLAY 39

// Bounce objects to easily and reliably read the buttons
Bounce buttonRecord = Bounce(BUTT_RECORD, 8);
Bounce buttonStop =   Bounce(BUTT_STOP, 8);  // 8 = 8 ms debounce time
Bounce buttonPlay =   Bounce(BUTT_PLAY, 8);


// which input on the audio shield will be used?
const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;



// Use these with the Teensy Audio Shield
#ifdef AUDIOSHIELD
  #define SDCARD_CS_PIN    10
  #define SDCARD_MOSI_PIN  7
  #define SDCARD_SCK_PIN   14
#else
  // Use these with the Teensy 3.5 & 3.6 SD card
  #define SDCARD_CS_PIN    BUILTIN_SDCARD
  #define SDCARD_MOSI_PIN  11  // not actually used
  #define SDCARD_SCK_PIN   13  // not actually used
#endif

// Use these for the SD+Wiz820 or other adaptors
//#define SDCARD_CS_PIN    4
//#define SDCARD_MOSI_PIN  11
//#define SDCARD_SCK_PIN   13



#define STOPPED 0
#define RECORDING 1
#define PLAYING 2

// Remember which mode we're doing
int mode = 0;  // 0=stopped, 1=recording, 2=playing

// The file where data is recorded
File frec;

void setup() {
  // Configure the pushbutton pins
  pinMode(BUTT_RECORD, INPUT_PULLUP);
  pinMode(BUTT_STOP, INPUT_PULLUP);
  pinMode(BUTT_PLAY, INPUT_PULLUP);

  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(60);

  #ifdef AUDIOSHIELD
    // Enable the audio shield, select input, and enable output
    sgtl5000_1.enable();
    sgtl5000_1.inputSelect(myInput);
    sgtl5000_1.volume(0.5);
  #endif

  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
}


void loop() {
  // First, read the buttons
  buttonRecord.update();
  buttonStop.update();
  buttonPlay.update();

// ---- AF POC with one pot...   
   if(MMODE==0) {
       startRecording();
       Serial.println("Started recording");
       for(int j = 0 ; j<15000;j++) {
          continueRecording();
          delay(1);      
       }
       stopRecording();
       Serial.println("Ended recording");
       delay(100000000);
   } else if (MMODE==1) {
      Serial.println("Started playing");
      startPlaying();      
      for(int j = 0 ; j<5000;j++) {
          continuePlaying();
          delay(10);      
       }
       stopPlaying();
       Serial.println("Ended playing");
       delay(100000000);
   }
//  -------
   
// Respond to button presses
  if (buttonRecord.fallingEdge()) {
    Serial.println("Record Button Press");
    if (mode == PLAYING) stopPlaying();
    if (mode == STOPPED) startRecording();
  }
  if (buttonStop.fallingEdge()) {
    Serial.println("Stop Button Press");
    if (mode == RECORDING) stopRecording();
    if (mode == PLAYING) stopPlaying();
  }
  if (buttonPlay.fallingEdge()) {
    Serial.println("Play Button Press");
    if (mode == RECORDING) stopRecording();
    if (mode == STOPPED) startPlaying();
  }

  // If we're playing or recording, carry on...
  if (mode == RECORDING) {
    continueRecording();
  }
  if (mode == PLAYING) {
    continuePlaying();
  }

  // when using a microphone, continuously adjust gain
  if (myInput == AUDIO_INPUT_MIC) adjustMicLevel();
}


void startRecording() {
  Serial.println("startRecording");
  if (SD.exists("RECORD.RAW")) {
    // The SD library writes new data to the end of the
    // file, so to start a new recording, the old file
    // must be deleted before new data is written.
    SD.remove("RECORD.RAW");
  }
  frec = SD.open("RECORD.RAW", FILE_WRITE);
  if (frec) {
    queue1.begin();
    mode = RECORDING;
  }
}

void continueRecording() {
  if (queue1.available() >= 2) {
    byte buffer[512];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    // write all 512 bytes to the SD card
    elapsedMicros usec = 0;
    frec.write(buffer, 512);
    // Uncomment these lines to see how long SD writes
    // are taking.  A pair of audio blocks arrives every
    // 5802 microseconds, so hopefully most of the writes
    // take well under 5802 us.  Some will take more, as
    // the SD library also must write to the FAT tables
    // and the SD card controller manages media erase and
    // wear leveling.  The queue1 object can buffer
    // approximately 301700 us of audio, to allow time
    // for occasional high SD card latency, as long as
    // the average write time is under 5802 us.
    //Serial.print("SD write, us=");
    //Serial.println(usec);
  }
}

void stopRecording() {
  Serial.println("stopRecording");
  queue1.end();
  if (1) {
    while (queue1.available() > 0) {
      frec.write((byte*)queue1.readBuffer(), 256);
      queue1.freeBuffer();
    }
    frec.close();
  }
  mode = STOPPED;
}


void startPlaying() {
  Serial.println("startPlaying");
  playRaw1.play("RECORD.RAW");
  mode = PLAYING;
}

void continuePlaying() {
  if (!playRaw1.isPlaying()) {
    playRaw1.stop();
    mode = STOPPED;
  }
}

void stopPlaying() {
  Serial.println("stopPlaying");
  playRaw1.stop();
  mode = STOPPED;
}

void adjustMicLevel() {
  // TODO: read the peak1 object and adjust sgtl5000_1.micGain()
  // if anyone gets this working, please submit a github pull request :-)
}

