* RetroTicker Wiring
```

LED Matrix (8 x 64 Pixel)

D5	->	CLK			(	IO, SCK		)
D7	->	DIN			(	IO, MOSI	)
D8	->	CS			(	IO, SS		)

WS2812B RGB LEDs (3 at the top if you want)

D2	->	DIN

Light Dependant Resistor

ADC	->	LDR
3V	->	LDR

```