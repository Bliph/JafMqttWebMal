# JafMqttWebMal

Enkel
1. Instasier JafMqttWeb
2. Kall setup()
3. Kall loop()

Override:
1. Lag ny klasse K som arver JafMqttWeb
2.a Override f.eks. handleRead
2.b Override f.eks. loopDataRead
2.c Override f.eks. loopMqttPublish
2.d Override f.eks. mqttCallback
3. Kall setup()
4. Kall loop()
