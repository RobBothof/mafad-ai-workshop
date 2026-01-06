
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

// Get values from model settings
uint32_t capture_size = EI_CLASSIFIER_SLICE_SIZE;
int number_of_labels = EI_CLASSIFIER_LABEL_COUNT;

// In order to improve detection stability, we sum and average 
// the classification scores over multiple inferences.
// We use the following variables to keep track of the summed scores.
static float summed_scores[EI_CLASSIFIER_LABEL_COUNT];
float max_score = 0.0f;
int best_label = -1;

// Create microphone object
i2sMic mic;

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
}

void loop()
{
    // A stream is not a real continuous signal but rather a series of
    // blocks/slices of audio data. We wait here until a now chunk of
    // audio data has come in from the microphone.
    mic.waitForStream();

    // Pack the audio data into a 'signal' that is suitable for the classifier.
    signal_t signal;
    signal.total_length = mic.getStreamSize();
    signal.get_data = &i2sMic::getStreamData;

    // Run the classifier on the collected audio data and store the classification in 'result'.
    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR error = run_classifier_continuous(&signal, &result, false);

    if (error != EI_IMPULSE_OK) {
        Serial.print("Failed to run classifier :");
        Serial.println(error);
        return;
    }

    // reset the score tracking variables to calculate the new winner
    max_score = 0;
    best_label = -1;

    // Print the averaged score for each label
    for (size_t ix = 0; ix < number_of_labels; ix++) {
        summed_scores[ix] = 0.5f * summed_scores[ix] + result.classification[ix].value;

        Serial.print(" ");
        Serial.print(result.classification[ix].label);
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
        Serial.print(result.classification[best_label].label);
        Serial.print(" ( ");
        Serial.print(max_score, 3);
        Serial.print(" ) **");
    }
}
