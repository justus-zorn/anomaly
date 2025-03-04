// Copyright 2023 Justus Zorn

#include <Anomaly.h>
#include <Client/Client.h>

#include <SDL.h>

Client::Client(Window& window) {
	if (enet_initialize() < 0) {
		window.error("Network initialization failed");
		throw std::exception();
	}
	host = enet_host_create(nullptr, 1, NET_CHANNELS, 0, 0);
	if (host == nullptr) {
		window.error("Could not create network socket");
		throw std::exception();
	}
}

Client::~Client() {
	if (peer != nullptr) {
		enet_peer_disconnect_now(peer, 0);
	}
	enet_host_destroy(host);
	enet_deinitialize();
}

bool Client::connect(Window& window, const std::string& hostname, uint16_t port) {
	ENetAddress address = { 0 };
	if (enet_address_set_host(&address, hostname.c_str()) < 0) {
		window.error("Could not resolve hostname '" + hostname + "'");
		return false;
	}
	address.port = port;
	peer = enet_host_connect(host, &address, NET_CHANNELS, 0);
	ENetEvent event;
	if (enet_host_service(host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		uint8_t login_packet[] = {
#ifdef ANOMALY_MOBILE
			1
#else
			0
#endif
		};
		ENetPacket* packet = enet_packet_create(login_packet, sizeof(login_packet), ENET_PACKET_FLAG_RELIABLE);
		enet_peer_send(peer, INPUT_CHANNEL, packet);
	}
	else {
		window.error("Could not connect to '" + hostname + ":[" + std::to_string(port) + "]'");
		return false;
	}
	return true;
}

bool Client::update(Audio& audio, Renderer& renderer) {
	ENetPacket* input_packet = renderer.get_window().create_input_packet();
	if (input_packet) {
		enet_peer_send(peer, INPUT_CHANNEL, input_packet);
	}
	ENetEvent event;
	while (enet_host_service(host, &event, 0) > 0) {
		switch (event.type) {
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
		case ENET_EVENT_TYPE_DISCONNECT:
			return false;
		case ENET_EVENT_TYPE_RECEIVE:
			if (event.channelID == SPRITE_CHANNEL) {
				draw(renderer, event.packet);
			}
			else if (event.channelID == COMMAND_CHANNEL) {
				handle_commands(renderer, event.packet);
			}
			else if (event.channelID == CONTENT_CHANNEL) {
				update_content(audio, renderer, event.packet);
			}
			else if (event.channelID == AUDIO_CHANNEL) {
				handle_audio(audio, event.packet);
			}
			enet_packet_destroy(event.packet);
			break;
		}
	}
	return true;
}

void Client::draw(Renderer& renderer, ENetPacket* packet) {
	renderer.clear(0.0f, 0.0f, 0.0f);
	uint32_t length = read32(packet->data);
	uint8_t* data = packet->data + 4;
	for (uint32_t i = 0; i < length; ++i) {
		uint32_t id = read32(data);
		float x = read_float(data + 4);
		float y = read_float(data + 8);
		float scale = read_float(data + 12);
		if (id & 0x80000000) {
			uint8_t r, g, b;
			r = data[16];
			g = data[17];
			b = data[18];
			uint32_t length = read32(data + 19);
			renderer.draw_text(id & ~0x80000000, x, y, scale, r, g, b, data + 23, length);
			data += 23;
			data += length;
		}
		else {
			renderer.draw_sprite(id, x, y, scale);
			data += 16;
		}
	}
	renderer.present();
}

void Client::handle_commands(Renderer& renderer, ENetPacket* packet) {
	uint32_t size = read32(packet->data);
	uint8_t* data = packet->data + 4;
	for (uint32_t i = 0; i < size; ++i) {
		uint8_t type = data[i];
		if (type == static_cast<uint8_t>(Command::Type::START_TEXT_INPUT)) {
			renderer.get_window().start_text_input();
		}
		else if (type == static_cast<uint8_t>(Command::Type::STOP_TEXT_INPUT)) {
			renderer.get_window().stop_text_input();
		}
	}
}

void Client::handle_audio(Audio& audio, ENetPacket* packet) {
	uint32_t size = read32(packet->data);
	uint8_t* data = packet->data + 4;
	for (uint32_t i = 0; i < size; ++i) {
		uint32_t id = read32(data);
		uint16_t channel = read16(data + 4);
		uint8_t volume = data[6];
		AudioCommand::Type type = static_cast<AudioCommand::Type>(data[7]);
		data += 8;
		audio.perform_command(id, channel, volume, type);
	}
}

void Client::update_content(Audio& audio, Renderer& renderer, ENetPacket* packet) {
	uint32_t id = read32(packet->data + 1);
	uint32_t length = read32(packet->data + 5);
	if (packet->data[0] == static_cast<uint8_t>(ContentType::IMAGE)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Received content update (image ID %u)", id);
		renderer.load_image(id, packet->data + 9, length);
	}
	else if (packet->data[0] == static_cast<uint8_t>(ContentType::FONT)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Received content update (font ID %u)", id);
		renderer.load_font(id, packet->data + 9, length);
	}
	else if (packet->data[0] == static_cast<uint8_t>(ContentType::SOUND)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Received content update (sound ID %u)", id);
		audio.load_sound(id, packet->data + 9, length);
	}
}
