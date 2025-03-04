// Copyright 2023 Justus Zorn

#include <glad/glad.h>

#include <Anomaly.h>
#include <Renderer/Window.h>

Window::Window() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		error(SDL_GetError());
	}
#ifdef ANOMALY_MOBILE
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeRight LandscapeLeft");
	window = SDL_CreateWindow("Anomaly", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
#else
	window = SDL_CreateWindow("Anomaly", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
#endif
	if (window == nullptr) {
		error(SDL_GetError());
	}
#ifdef ANOMALY_MOBILE
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	context = SDL_GL_CreateContext(window);
	if (!context) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), window);
		exit(1);
	}
#ifdef ANOMALY_MOBILE
	if (!gladLoadGLES2Loader(SDL_GL_GetProcAddress)) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not initialize OpenGL", window);
		exit(1);
	}
#else
	if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not initialize OpenGL", window);
		exit(1);
	}
#endif
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

Window::~Window() {
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Window::error(const std::string& message) {
	if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
	}
}

bool Window::update() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			return false;
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
				int width, height;
				SDL_GL_GetDrawableSize(window, &width, &height);
				glViewport(0, 0, width, height);
				break;
			}
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_BACKSPACE) {
				while (input.composition.length() > 0 && (input.composition.back() & 0xC0) == 0x80) {
					input.composition.pop_back();
				}
				if (input.composition.length() > 0) {
					input.changed_composition = true;
					input.composition.pop_back();
				}
			}
			input.key_events.push_back({ event.key.keysym.sym, true });
			break;
		case SDL_KEYUP:
			input.key_events.push_back({ event.key.keysym.sym, false });
			break;
#ifdef ANOMALY_MOBILE
		case SDL_FINGERDOWN:
			input.mouse_events.push_back({ (event.tfinger.x * 2.0f - 1.0f) * aspect_ratio(),
				-event.tfinger.y * 2.0f + 1.0f, static_cast<uint8_t>(event.tfinger.fingerId),
				static_cast<uint8_t>(InputEventType::DOWN) });
			break;
		case SDL_FINGERUP:
			input.mouse_events.push_back({ (event.tfinger.x * 2.0f - 1.0f) * aspect_ratio(),
				-event.tfinger.y * 2.0f + 1.0f, static_cast<uint8_t>(event.tfinger.fingerId),
				static_cast<uint8_t>(InputEventType::UP) });
			break;
		case SDL_FINGERMOTION:
			input.mouse_events.push_back({ (event.tfinger.x * 2.0f - 1.0f) * aspect_ratio(),
				-event.tfinger.y * 2.0f + 1.0f, static_cast<uint8_t>(event.tfinger.fingerId),
				static_cast<uint8_t>(InputEventType::MOTION) });
			break;
#else
		case SDL_MOUSEBUTTONDOWN:
			input.mouse_events.push_back({ (event.button.x / width() * 2.0f - 1.0f) * aspect_ratio(),
				-event.button.y / height() * 2.0f + 1.0f,
				static_cast<uint8_t>(event.button.button),
				static_cast<uint8_t>(InputEventType::DOWN) });
			break;
		case SDL_MOUSEBUTTONUP:
			input.mouse_events.push_back({ (event.button.x / width() * 2.0f - 1.0f) * aspect_ratio(),
				-event.button.y / height() * 2.0f + 1.0f,
				static_cast<uint8_t>(event.button.button),
				static_cast<uint8_t>(InputEventType::UP) });
			break;
		case SDL_MOUSEMOTION:
			input.mouse_events.push_back({ (event.button.x / width() * 2.0f - 1.0f) * aspect_ratio(),
				-event.button.y / height() * 2.0f + 1.0f, 0,
				static_cast<uint8_t>(InputEventType::MOTION) });
			break;
		case SDL_MOUSEWHEEL:
			input.wheel_x += event.wheel.preciseX;
			input.wheel_y += event.wheel.preciseY;
			break;
#endif
		case SDL_TEXTINPUT:
			input.composition += event.text.text;
			input.changed_composition = true;
			break;
		}
	}
	return true;
}

void Window::present() {
	SDL_GL_SwapWindow(window);
}

float Window::aspect_ratio() const {
	return width() / height();
}

ENetPacket* Window::create_input_packet() {
	return input.create_input_packet();
}

void Window::start_text_input() {
	if (!SDL_IsTextInputActive()) {
		SDL_StartTextInput();
	}
}

void Window::stop_text_input() {
	SDL_StopTextInput();
	input.composition = "";
	input.changed_composition = true;
}

float Window::width() const {
	int width;
	SDL_GL_GetDrawableSize(window, &width, nullptr);
	return width;
}

float Window::height() const {
	int height;
	SDL_GL_GetDrawableSize(window, nullptr, &height);
	return height;
}
