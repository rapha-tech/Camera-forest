# Image processing and phenological analysis (From camera to LoRa)
## Overall objective:

The objective of this sub-project is to design and implement the complete software chain enabling the automatic analysis of photographs of the forest area captured by the on-board camera, in order to quantify the phenology of the vegetation (create the RGB histogram of the image) and to transmit the results autonomously via a low-power wireless network (LoRa).

## Details of the treatment steps :

1. Acquisition and loading of visual streams


2. Applying the mask to the forest image


3. RGB histogram calculation & average intensity


4. Calculation of the normalized average vegetation index (ExG)


5. Analyse statistique et extraction d'indicateurs (Histogramme et Moyenne de l'ExG)


6. LoRa formatting and transmission
