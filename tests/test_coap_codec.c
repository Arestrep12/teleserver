#include "coap.h"
#include "coap_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_basic_message(CoapMessage *msg) {
	coap_message_init(msg);
	msg->type = COAP_TYPE_CONFIRMABLE;
msg->code = COAP_METHOD_GET;
	msg->message_id = 0x1234;
	msg->token_length = 2;
	msg->token[0] = 0xDE; msg->token[1] = 0xAD;

	const char *p1 = "sensor";
	const char *p2 = "temp";
	assert(coap_message_add_option(msg, COAP_OPTION_URI_PATH,
						(const uint8_t *)p1, strlen(p1)) == 0);
	assert(coap_message_add_option(msg, COAP_OPTION_URI_PATH,
						(const uint8_t *)p2, strlen(p2)) == 0);
	// Accept: JSON (50)
	uint8_t acc = 50;
	assert(coap_message_add_option(msg, COAP_OPTION_ACCEPT, &acc, 1) == 0);
	// payload "42"
	msg->payload = msg->payload_buffer;
	msg->payload_length = 2;
	msg->payload_buffer[0] = '4';
	msg->payload_buffer[1] = '2';
}

static void assert_messages_equal(const CoapMessage *a, const CoapMessage *b) {
	assert(a->version == b->version);
	assert(a->type == b->type);
	assert(a->token_length == b->token_length);
	assert(memcmp(a->token, b->token, a->token_length) == 0);
	assert(a->code == b->code);
	assert(a->message_id == b->message_id);
	assert(a->option_count == b->option_count);
	for (size_t i = 0; i < a->option_count; i++) {
		assert(a->options[i].number == b->options[i].number);
		assert(a->options[i].length == b->options[i].length);
		assert(memcmp(a->options[i].value,
				   b->options[i].value,
				   a->options[i].length) == 0);
	}
	assert(a->payload_length == b->payload_length);
	if (a->payload_length > 0) {
		assert(memcmp(a->payload, b->payload, a->payload_length) == 0);
	}
}

static void test_round_trip_basic(void) {
	CoapMessage msg;
	build_basic_message(&msg);

	uint8_t buf[COAP_MAX_MESSAGE_SIZE];
	int n = coap_encode(&msg, buf, sizeof(buf));
	assert(n > 0);

	CoapMessage dec;
	coap_message_init(&dec);
	int rc = coap_decode(&dec, buf, (size_t)n);
	assert(rc == 0);

	assert_messages_equal(&msg, &dec);
	printf("✓ test_round_trip_basic\n");
}

static void test_extension_13_delta(void) {
	CoapMessage msg;
	coap_message_init(&msg);
msg.code = COAP_METHOD_GET;
	// If-Match (1) con 1 byte
	uint8_t etag = 0xAA;
	assert(coap_message_add_option(&msg, COAP_OPTION_IF_MATCH, &etag, 1) == 0);
	// Max-Age (14) => delta 13
	uint8_t max_age = 0x00;
	assert(coap_message_add_option(&msg, COAP_OPTION_MAX_AGE, &max_age, 1) == 0);

	uint8_t buf[128];
	int n = coap_encode(&msg, buf, sizeof(buf));
	assert(n > 0);

	CoapMessage dec; coap_message_init(&dec);
	assert(coap_decode(&dec, buf, (size_t)n) == 0);
	assert_messages_equal(&msg, &dec);
	printf("✓ test_extension_13_delta\n");
}

static void test_extension_14_length(void) {
	CoapMessage msg; coap_message_init(&msg);
msg.code = COAP_METHOD_GET;
	// Uri-Query con 270 bytes
	uint8_t big[270];
	for (size_t i = 0; i < sizeof(big); i++) big[i] = (uint8_t)('a' + (i % 26));
	assert(coap_message_add_option(&msg, COAP_OPTION_URI_QUERY, big, sizeof(big)) == 0);

	uint8_t buf[2048];
	int n = coap_encode(&msg, buf, sizeof(buf));
	assert(n > 0);
	CoapMessage dec; coap_message_init(&dec);
	assert(coap_decode(&dec, buf, (size_t)n) == 0);
	assert_messages_equal(&msg, &dec);
	printf("✓ test_extension_14_length\n");
}

static void test_invalid_tkl_decode(void) {
	// Construir un mensaje bruto con TKL=9
	uint8_t raw[] = { 0,0,0,0 };
	raw[0] = (uint8_t)((COAP_VERSION << 6) | (COAP_TYPE_CONFIRMABLE << 4) | 9);
	raw[1] = COAP_METHOD_GET; raw[2] = 0x12; raw[3] = 0x34;
	CoapMessage msg; coap_message_init(&msg);
	int rc = coap_decode(&msg, raw, sizeof(raw));
	assert(rc < 0);
	printf("✓ test_invalid_tkl_decode\n");
}

static void test_invalid_nibble_15_decode(void) {
	// Mensaje con una opción cuyo primer byte tiene nibble 15 en delta
	uint8_t raw[8]; memset(raw, 0, sizeof(raw));
	raw[0] = (uint8_t)((COAP_VERSION << 6) | (COAP_TYPE_CONFIRMABLE << 4) | 0);
	raw[1] = COAP_METHOD_GET; raw[2] = 0; raw[3] = 1;
	// Opción con delta=15, length=0
	raw[4] = 0xF0; // delta 15, len 0
	CoapMessage msg; coap_message_init(&msg);
	int rc = coap_decode(&msg, raw, 5);
	assert(rc < 0);
	printf("✓ test_invalid_nibble_15_decode\n");
}

static void test_unordered_options_encode_error(void) {
	CoapMessage msg; coap_message_init(&msg);
	msg.code = COAP_METHOD_GET;
	// Añadimos fuera de orden manualmente en la estructura (evitar helper)
msg.options[0].number = 11; msg.options[0].length = 1; msg.options[0].value[0] = 'a';
	msg.options[1].number = 3;  msg.options[1].length = 1; msg.options[1].value[0] = 'b';
	msg.option_count = 2;
	uint8_t buf[64];
	int n = coap_encode(&msg, buf, sizeof(buf));
	assert(n < 0);
	printf("✓ test_unordered_options_encode_error\n");
}

static void test_length_over_270_decode_error(void) {
	// Opción con length = 271 (nibble 14 y ext=2)
	uint8_t raw[16]; memset(raw, 0, sizeof(raw));
	raw[0] = (uint8_t)((COAP_VERSION << 6) | (COAP_TYPE_CONFIRMABLE << 4));
	raw[1] = COAP_METHOD_GET; raw[2] = 0; raw[3] = 1;
	// Primera opción: delta=0 (0), len=14 con ext=0x0002 => 269+2 = 271
	raw[4] = (uint8_t)((0 << 4) | 14);
	raw[5] = 0x00; raw[6] = 0x02;
	CoapMessage msg; coap_message_init(&msg);
	int rc = coap_decode(&msg, raw, 7);
	assert(rc < 0);
	printf("✓ test_length_over_270_decode_error\n");
}

static void test_small_output_buffer_encode_error(void) {
	CoapMessage msg; coap_message_init(&msg);
msg.code = COAP_METHOD_GET; msg.token_length = 0;
	uint8_t buf[3];
	int n = coap_encode(&msg, buf, sizeof(buf));
	assert(n == COAP_CODEC_E2SMALL);
	printf("✓ test_small_output_buffer_encode_error\n");
}

static void test_no_payload_marker_when_empty(void) {
	CoapMessage msg; coap_message_init(&msg);
msg.code = COAP_METHOD_GET;
	uint8_t buf[128];
	int n = coap_encode(&msg, buf, sizeof(buf));
	assert(n > 0);
	// Asegurar que no haya 0xFF
	int found = 0; for (int i = 0; i < n; i++) if (buf[i] == 0xFF) { found = 1; break; }
	assert(found == 0);
	printf("✓ test_no_payload_marker_when_empty\n");
}

int main(void) {
	printf("=== Tests de codec CoAP ===\n");

	test_round_trip_basic();
	test_extension_13_delta();
	test_extension_14_length();
	test_invalid_tkl_decode();
	test_invalid_nibble_15_decode();
	test_unordered_options_encode_error();
	test_length_over_270_decode_error();
	test_small_output_buffer_encode_error();
	test_no_payload_marker_when_empty();

	printf("✓ Todos los tests de codec pasaron\n");
	return 0;
}