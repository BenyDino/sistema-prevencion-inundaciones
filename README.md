# sistema-prevencion-inundaciones
Sistema Automatizado de Detección y Prevención de Inundaciones

1. Descripción del proyecto

Consiste en un prototipo basado en ESP32 que simula la detección de niveles de agua y activa respuestas automáticas mediante indicadores luminosos, una alarma, un servomotor y un relevador.

2. Objetivo

Desarrollar un sistema automatizado que permita identificar distintos niveles de riesgo de inundación y representar acciones preventivas mediante sensores y actuadores.

3. Componentes

* ESP32 DevKit
* Sensor ultrasónico HC-SR04
* Potenciómetro para simular la intensidad de lluvia
* Sensor DHT22
* Servomotor SG90
* Módulo relevador
* Zumbador (buzzer)
* LEDs indicadores
* Botón de emergencia E-STOP

4. Tecnologías utilizadas

* Wokwi
* ESP-IDF
* Lenguaje C
* ESP32

5. Archivos del proyecto

* diagram.json: contiene los componentes y las conexiones del circuito.
* main.c: contiene el programa del microcontrolador ESP32.

6. Simulación

https://wokwi.com/projects/477364295877862401

7. Estado del proyecto

El prototipo cuenta con una simulación inicial. Las funciones implementadas y las pruebas realizadas deben verificarse en el código y documentarse con evidencias. La integración MQTT y el dashboard de Node-RED deben presentarse como pendientes mientras no se hayan probado.

8. Seguridad

Esta es una simulación académica. Para una implementación física se deben considerar la adaptación de niveles lógicos, el aislamiento eléctrico, la alimentación de los actuadores y un circuito de parada de emergencia adecuado.
