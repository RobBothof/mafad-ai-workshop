
========================

ei-mafad-classifier-arduino-1.0.5.zip

Cropped
all classes
Window size: 1000ms
Stride: 500ms


Frame length: 0.025
Frame stride: 0.01
Filter number: 41
FFT length: 512
Low frequency: 80
High frequency: 8000
Noise floor (dB): -75

epoch: 100
learning rate: 0.05
CPU
validation size: 20%

validation:
Accuracy: 94%
Loss: 0.15

test:
Accuracy: 91.94%

hello there: ***--
ilovecake:   ***--
get bonus:   ***--
random:      ***--

distance     *----
soft-close   *----

========================

ei-mafad-classifier-arduino-1.0.8.zip

Cropped
excluded random class
Window size: 1000ms
Stride: 500ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 80
High frequency: 8000
Noise floor (dB): -50

epoch: 40
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 99%
Loss: 0.04

test:
Accuracy: 99.19%

mobile performance: **---
esp32 performance:  

========================

ei-mafad-classifier-arduino-1.0.13.zip

Cropped
all classes
Window size: 1000ms
Stride: 500ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 80
High frequency: 8000
Noise floor (dB): -50

epoch: 70
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 99.2%
Loss: 0.04

test:
Accuracy: 99.03%

hello there: **---
ilovecake:   ****-
get bonus:   ***--  
random:      ****-

========================

ei-mafad-classifier-arduino-1.0.22.zip
Cropped
excluded random class
Window size: 1000ms
Stride: 500ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 300
High frequency: 8000
Noise floor (dB): -65

epoch: 50
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 99.0%
Loss: 0.06

test:
Accuracy: 97.58%

hello there: *----
ilovecake:   ****-
get bonus:   ***--  
random:      -----

hello there performs worse at low volume somehow

========================

ei-mafad-classifier-arduino-1.0.14.zip

Master
all classes
Window size: 1000ms
Stride: 100ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 300
High frequency: 8000
Noise floor (dB): -60

epoch: 40
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 99.4%
Loss: 0.02

test:
Accuracy: 95.97%

hello there: ***__
ilovecake:   ****_
get bonus:   ****_ 
random:      ****_

========================

ei-mafad-classifier-arduino-1.0.16.zip
Master
all classes
Window size: 1000ms
Stride: 200ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 300
High frequency: 8000
Noise floor (dB): -60

epoch: 40
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 98.8%
Loss: 0.06

test:
Accuracy: 93.51%

hello there: *----
ilovecake:   **---
get bonus:   -----  
random:      ***--

========================
ei-mafad-classifier-arduino-1.0.18.zip

Master
excluded random class
Window size: 1000ms
Stride: 200ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 300
High frequency: 8000
Noise floor (dB): -60

epoch: 40
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 100%
Loss: 0.05

test:
Accuracy: 97.75%

mobile performance: ***--
esp32 performance:  


======================================

ei-mafad-improved-classifier-arduino-1.0.3.zip

Architecture changes

Model:

- Input layer (7,960 features)
- Reshape layer (40 columns)
- 2D conv / pool layer (8 filters, 3 kernel size, 1 layer)
- Dropout (rate 0.2)
- 2D conv / pool layer (16 filters, 3 kernel size, 1 layer)
- Flatten layer
- Output layer (4 classes)

Master
all classes
Window size: 1000ms
Stride: 200ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 300
High frequency: 8000
Noise floor (dB): -65

epoch: 14
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 99.2%
Loss: 0.05

test:
Accuracy: 94.56%


hello there: **---
get bonus:   ***--  
ilovecake:   ***--
random:      ****-

======================================
ei-mafad-improved-classifier-arduino-1.0.5

Architecture changes

Model:

- Input layer (7,960 features)
- Reshape layer (40 columns)
- 2D conv / pool layer (6 filters, 5 kernel size, 1 layer)
- Dropout (rate 0.2)
- 2D conv / pool layer (12 filters, 3 kernel size, 1 layer)
- Flatten layer
- Output layer (4 classes)

Master
no random class
Window size: 1000ms
Stride: 200ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 512
Low frequency: 300
High frequency: 8000
Noise floor (dB): -65

epoch: 15
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 99.0%
Loss: 0.05

test:
Accuracy: 97.72%

hello there:    ****-
get bonus:      ****-    
ilovecake:      ***--
random:         -----

softer sounds  ****_
long distance   ***__
======================================
GOOD:
ei-mafad-improved-classifier-arduino-1.0.7
Architecture changes

Model:

- Input layer (7,960 features)
- Reshape layer (40 columns)
- 2D conv / pool layer (6 filters, 5 kernel size, 1 layer)
- Dropout (rate 0.2)
- 2D conv / pool layer (12 filters, 3 kernel size, 1 layer)
- Flatten layer
- Output layer (4 classes)

Master
all classes
Window size: 1000ms
Stride: 50ms

Frame length: 0.01
Frame stride: 0.005
Filter number: 40
FFT length: 256
Low frequency: 300
High frequency: 8000
Noise floor (dB): -40

epoch: 10
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 98.4%
Loss: 0.06

test:
Accuracy: 98.41%

hello there: **---
ilovecake:   ***--
get bonus:   ****-    
random:      ****-


softer sounds **---
long distance **---

============================================================================
ei-mafad-improved-classifier-arduino-1.0.9.zip

Architecture changes

Model:

- Input layer (7,960 features)
- Reshape layer (40 columns)
- 2D conv / pool layer (6 filters, 5 kernel size, 1 layer)
- Dropout (rate 0.2)
- 2D conv / pool layer (12 filters, 3 kernel size, 1 layer)
- Flatten layer
- Output layer (4 classes)

Manual Crops
all classes
Window size: 1000ms
Stride: 200ms

Frame length: 0.02
Frame stride: 0.015
Filter number: 30
FFT length: 128
Low frequency: 300
High frequency: 8000
Noise floor (dB): -40

epoch: 20
learning rate: 0.02
CPU
validation size: 20%

validation:
Accuracy: 94.55%
Loss: 0.15

test:
Accuracy: 94.55%

hello there: ***--
ilovecake:   ***--
get bonus:   *----
random:      ****-
