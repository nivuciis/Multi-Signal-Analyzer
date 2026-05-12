#include "handles/handles_internal.h"

void handle_set_trigger(void)
{
	if (self.cmd_str_index < 3) {
		log_warn("sigrok", "Trigger command too short");
		return;
	}

	char type_char = (char)self.cmd_str[1];

	if (type_char == 'n') {
		self.trigger_config.trigger_mask = 0;
		log_inf("sigrok", "Trigger disabled");
		return;
	}

	int idx = (int)strtol((char *)&self.cmd_str[2], &self.end_ptr, 10);

	if (self.end_ptr == (char *)&self.cmd_str[2] || self.end_ptr == NULL ||
	    *self.end_ptr != '\0') {
		log_warn("sigrok", "Invalid trigger channel: %s", &self.cmd_str[2]);
		return;
	}

	int ch = idx - 2;

	if (ch < 0 || ch >= PICO_DEFAULT_CHANNELS_PIN_COUNT) {
		log_warn("sigrok", "Trigger channel out of range: %d", ch);
		return;
	}

	enum ana_trigger_type trig_type;
	switch (type_char) {
	case '0':
		trig_type = ANA_TRIGGER_LEVEL_LOW;
		break;
	case '1':
		trig_type = ANA_TRIGGER_LEVEL_HIGH;
		break;
	case '2':
		trig_type = ANA_TRIGGER_EDGE_RISE;
		break;
	case '3':
		trig_type = ANA_TRIGGER_EDGE_FALL;
		break;
	case '4':
		trig_type = ANA_TRIGGER_EDGE_BOTH;
		break;
	default:
		log_warn("sigrok", "Unknown trigger type: %c", type_char);
		return;
	}

	self.trigger_config.trigger_mask = (uint16_t)(1u << ch);
	self.trigger_config.trigger_type[ch] = trig_type;
	log_inf("sigrok", "Trigger set: ch %d type '%c'", ch, type_char);
}
