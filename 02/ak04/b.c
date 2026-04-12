/*
AK04 b)
Author:
	Ammerer Jakob
Library: libao 1.2.0
	URL:https://xiph.org/ao/
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ao/ao.h>

// Präprozessor Constants (replacing text with value before compiling)
#define SAMPLE_RATE 44100
#define BASE_FREQ 440
#define BPM         160
#define QUARTER     (60.0f / BPM)
#define EIGHTH      (QUARTER / 2.0f)
#define SIXTEENTH   (QUARTER / 4.0f)

// base tones in midi nr: https://inspiredacoustics.com/en/MIDI_note_numbers_and_center_frequencies
#define C  0
#define CS 1
#define D  2
#define DS 3
#define E  4
#define F  5
#define FS 6
#define G  7
#define GS 8
#define A  9
#define AS 10
#define B  11


// tones used for melody (to get frequencies initial call of midiToFreq(); foreach tone needed.
const float C4  = 261.63f;
const float D4  = 293.66f;
const float F4  = 349.23f;
const float G4  = 392.00f;
const float GS4 = 415.30f;
const float A4  = 440.00f;
const float D5  = 587.33f;

typedef struct {
    float freqInHz;
    float durationInSecs;
} Note;

// midi nr: https://inspiredacoustics.com/en/MIDI_note_numbers_and_center_frequencies
// https://musescore.com/user/27143663/scores/20245852
// to add a pause to the melody, i used frequency 0. You can see how i pause later in the function playNote();
static const Note melody[] = {
    { D4,   EIGHTH  },
    { D4,   EIGHTH  },
    { D5,   QUARTER },
    { A4,   QUARTER },
    { 0.0f, EIGHTH  },
    { GS4,  QUARTER },
    { 0.0f, EIGHTH  },
    { G4,   QUARTER },
    { F4,   QUARTER },
    { D4,   EIGHTH  },
    { F4,   EIGHTH  },
    { G4,   QUARTER },
    { D4,   EIGHTH  },
    { D4,   EIGHTH  },
    { D5,   QUARTER },
    { A4,   QUARTER },
    { 0.0f, EIGHTH  },
    { GS4,  QUARTER },
    { 0.0f, EIGHTH  },
    { G4,   QUARTER },
    { C4,   QUARTER },
    { C4,   EIGHTH  },
    { D4,   EIGHTH  },
    { F4,   QUARTER }
};

// function prototypes to promise the compiler said functions are available.
float midiToFreq(int octave, int semitone); // this function was used to get the specific frequencies for my tones for the melody cause i was not sure if we are allowed to just take those frequencies from the web.
void playNote(ao_device *device, float freq, float durationInSecs);

int main(int argc, char **argv){
    ao_device *device;
    ao_sample_format format;
    int default_driver;

	// init default audio ouput (code from ao_example.c)
    ao_initialize();
    default_driver = ao_default_driver_id();

    memset(&format, 0, sizeof(format));
    format.bits = 16;
    format.channels = 2;
    format.rate = SAMPLE_RATE;
    format.byte_format = AO_FMT_LITTLE;

	//int wav_driver = ao_driver_id("wav"); // these two lines export the sound as wav
	//device = ao_open_file(wav_driver, "Megalovania.wav", 1, &format, NULL); // used to export the result as a .wav file
    device = ao_open_live(default_driver, &format, NULL);
    if (device == NULL) {
        fprintf(stderr, "Error opening device.\n");
        return 1;
    }

	// play melody
	int notesCount = sizeof(melody) / sizeof(melody[0]); //size of array in byte / size of one element to get the count
	for(int i = 0; i < notesCount; i++){
		playNote(device, melody[i].freqInHz, melody[i].durationInSecs);
	}

    ao_close(device);
    ao_shutdown();
    return 0;
}
void playNote(ao_device *device, float freq, float durationInSecs) {
 	// Guard against null
 	if(device == NULL) return;
 	if(durationInSecs == 0.0f) return;

	float bit16audio = 32768.0; 								// 16 bit audio (Wertebereich -32768 bis +32767) multiplying with this to use the full range. this is max for 16 bit
	float amp = 0.75; 											// amplitude 75% to prevent clipping if sin is +- 1
 	int samplesCount = (int)(SAMPLE_RATE * durationInSecs); 	// Sample Count for each note that is passed in function
 	int bufferSize = (4 * samplesCount); 						// 4 bytes pro sample for buffer size
 	char *buffer = calloc(bufferSize, sizeof(char)); 			// allocate memory and init with 0
 	int sample = 0; 											// standard init with 0

 	for(int i = 0; i < samplesCount; i++){
		if(freq > 0){
			// sample calculation from ao_example.c adjusted for my usecase
			// this is used to "pause". Later the buffer get filled with zeros which means silence
			sample = (int)(
				amp *
				bit16audio *
				sin(2 * M_PI * freq * i / SAMPLE_RATE)
			); // prev. i used sample count which led to every tone sounding the same
		}
		else{
			// here it pauses. buffer already 0 due to using calloc not malloc
			sample = 0;
		}

		/*
			Info: For this comment i used CLAUDE SONNET 4.6,
				usecase: to give me a better understanding of what the following two lines from the ao_example.c are doing.

			Ein 16-bit Sample (2 Bytes) muss für Stereo (2 Kanäle) in 4 Buffer-Slots geschrieben werden:
			[4*i]   = linker Kanal,  unteres Byte  (sample & 0xff)
			[4*i+1] = linker Kanal,  oberes Byte   (sample >> 8)
			[4*i+2] = rechter Kanal, unteres Byte  (gleich wie links → Mono auf Stereo dupliziert)
			[4*i+3] = rechter Kanal, oberes Byte
		*/
		buffer[4*i]   = buffer[4*i+2] = sample & 0xff;
		buffer[4*i+1] = buffer[4*i+3] = (sample >> 8) & 0xff;
	}
 	ao_play(device, buffer, bufferSize);
    free(buffer);
    }

float midiToFreq(int octave, int semitone){
	// source: https://caml.music.mcgill.ca/~gary/307/week1/node11.html
    // formula: f(n) = 440 * 2^((n - 69) / 12) ... n - 69 = diffrence in halftones form A4.
    // n = MidiNr = (octave + 1) * 12 + semitone
    int midi = (octave + 1) * 12 + semitone;
    int freq  = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
    return freq;
}

