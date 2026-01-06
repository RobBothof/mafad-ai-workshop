
// Include the model from Edge Impulse
#define EIDSP_QUANTIZE_FILTERBANK   0
#include <MAFAD_Classifier_inferencing.h>

// Include the AI Workshop libraries
#include <ai-workshop-main.h>
#include <ai-workshop-ws2812.h>
#include <ai-workshop-mic.h>

// Microphone pins
#define MIC_SCK_PIN 1   // clockPin
#define MIC_WS_PIN 7    // wordSelectPin
#define MIC_SD_PIN 10   // channelSelectPin

#define LEDRING_PIN 43   // ledring data pin
#define NUM_LEDS 8 // the ledring uses 8 leds

// Create an array/list to store 8 colors for the ledring.
Color leds[NUM_LEDS];

// Create a led ring object.
WS2812 ledRing;

uint8_t activeLED = 0;
uint32_t ledSequenceTimer = 0;

// Get values from model settings
uint32_t capture_size = EI_CLASSIFIER_SLICE_SIZE;
int number_of_labels = EI_CLASSIFIER_LABEL_COUNT;

// Share the classification results
volatile ei_impulse_result_t classifier_results;
volatile bool classifier_updated = false;

// In order to improve detection stability, we sum and average 
// the classification scores over multiple inferences.
// We use the following variables to keep track of the summed scores.
static float summed_scores[EI_CLASSIFIER_LABEL_COUNT];
float max_score = 0.0f;
int best_label = -1;

// Create microphone object
i2sMic mic;


// We create a Task, that can run on the second CPU core,
// So we can things in parallel and have room on the first 
// core for other tasks (like controlling LEDs)
void inferenceTask(void* param) {
    while (true) {

        // A stream is not a real continuous signal but rather a series of
        // blocks/slices of audio data. We wait here until a now chunk of
        // audio data has come in from the microphone.
        if (mic.isStreamReady()) {
            mic.consumeStream(); // resets bufferReady

            // Pack the audio data into a 'signal' that is suitable for the classifier.
            signal_t signal;
            signal.total_length = mic.getStreamSize();
            signal.get_data = &i2sMic::getStreamData;

            // Run the classifier on the collected audio data and store the classification in 'result'.
            ei_impulse_result_t result = {0};
            EI_IMPULSE_ERROR error = run_classifier_continuous(&signal, &result, false);

            // If there is an error, we just skip this round
            if (error != EI_IMPULSE_OK) {
                continue;
            }

            // only update the shared result if the old results have been consumed.
            if (classifier_updated == false) {
                memcpy((void*)&classifier_results, (void*)&result, sizeof(ei_impulse_result_t));
                classifier_updated = true;
            }

        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void setup()
{
    // Start serial printing for debugging
    Serial.begin(115200);
    Serial.println("--------------------");
    Serial.println("* Model Inference Test *");

    // Initialize the classifier / model
    run_classifier_init();

    // Set up the microphone and start audio stream for inference
    mic.setup(MIC_SCK_PIN, MIC_WS_PIN, MIC_SD_PIN);
    mic.startStream(capture_size); 

    // Clear the summed score for each label
    for (int i = 0; i< number_of_labels; i++) {
        summed_scores[i] = 0;
    }

    // Setup LedRing
    pinMode(LEDRING_PIN, OUTPUT);
    ledRing.init(LEDRING_PIN, leds, NUM_LEDS);
    ledRing.clear();
    ledRing.update();

    // Start inference task on the other CPU core
    xTaskCreatePinnedToCore(inferenceTask, "InferenceTask", 8192, NULL, 2, NULL, 0); // 0 = core 0

}

void loop()
{
 
    if (classifier_updated) {
        // reset the score tracking variables to calculate the new winner
        max_score = 0;
        best_label = -1;

        // Print the averaged score for each label
        for (size_t ix = 0; ix < number_of_labels; ix++) {
            summed_scores[ix] = 0.5f * summed_scores[ix] + classifier_results.classification[ix].value;

            Serial.print(" ");
            Serial.print(classifier_results.classification[ix].label);
            Serial.print(" ");
            Serial.print(summed_scores[ix], 3); // print with 3 decimal places
            Serial.print("  ");

            if (summed_scores[ix] > max_score) {
                max_score = summed_scores[ix];
                best_label = ix;
            }
        }

        // Print the best label and its score (if it is one of the melody labels)
        if (best_label == 0 || best_label == 1 || best_label == 2) {
            Serial.print("** top = ");
            Serial.print(classifier_results.classification[best_label].label);
            Serial.print(" ( ");
            Serial.print(max_score, 3);
            Serial.print(" ) **");
        }
        classifier_updated = false;
        Serial.println();
    }
    delay(1);

    if (best_label == 0 || best_label == 1 || best_label == 2) {
        ledRing.clear();
        for (int l=0; l<NUM_LEDS;l++) {
            if (best_label==0) {
                ledRing[l].hex = 0x660000;
            }
            if (best_label==1) {
                ledRing[l].hex = 0x006600;
            }
            if (best_label==2) {
                ledRing[l].hex = 0x000066;
            }

        }
        ledRing.update();

    } 
    else 
    {
        // show the led sequence, scanning
        if (millis() - ledSequenceTimer > 200) {
            // Update LED ring sequence
            ledRing.clear();
            ledRing[activeLED].hex = 0x330000;
            ledRing.update();

            activeLED++;
            if (activeLED >= NUM_LEDS) {
                activeLED = 0;
            }
            ledSequenceTimer = millis();
        }
    }

}
