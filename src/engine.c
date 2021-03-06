typedef struct {
  bool isPressed;
} KEY_STATE;

typedef struct {
  KEY_STATE left;
  KEY_STATE right;
  KEY_STATE up;
  KEY_STATE down;
  KEY_STATE space;
} INPUT_STATE;

typedef struct {
  SDL_Window* window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  void* pixels;
  INPUT_STATE keyboard;
  ABC_FIFO fifo;
  ForeignFunctionMap fnMap;
} ENGINE;

typedef enum {
  EVENT_NOP,
  EVENT_LOAD_FILE
} EVENT_TYPE;

typedef enum {
  TASK_NOP,
  TASK_PRINT,
  TASK_LOAD_FILE
} TASK_TYPE;

global_variable uint32_t ENGINE_EVENT_TYPE;

internal int
ENGINE_taskHandler(ABC_TASK* task) {
	if (task->type == TASK_PRINT) {
		printf("%s\n", (char*)task->data);
		task->resultCode = 0;
		// TODO: Push to SDL Event Queue
	} else if (task->type == TASK_LOAD_FILE) {
    GAMEFILE* file = (GAMEFILE*)task->data;
    char* fileData = readEntireFile(file->name);
    // printf("%s\n", file->name);

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = ENGINE_EVENT_TYPE;
    event.user.code = EVENT_LOAD_FILE;
    event.user.data1 = file;
    event.user.data2 = fileData;
    SDL_PushEvent(&event);
  }
  return 0;
}

internal int
ENGINE_init(ENGINE* engine) {
  int result = EXIT_SUCCESS;
  engine->window = NULL;
  engine->renderer = NULL;
  engine->texture = NULL;
  engine->pixels = NULL;

  //Create window
  engine->window = SDL_CreateWindow("DOME", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
  if(engine->window == NULL)
  {
    SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    result = EXIT_FAILURE;
    goto engine_init_end;
  }

  engine->renderer = SDL_CreateRenderer(engine->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (engine->renderer == NULL)
  {
    SDL_Log("Could not create a renderer: %s", SDL_GetError());
    result = EXIT_FAILURE;
    goto engine_init_end;
  }
  SDL_RenderSetLogicalSize(engine->renderer, GAME_WIDTH, GAME_HEIGHT);

  engine->texture = SDL_CreateTexture(engine->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GAME_WIDTH, GAME_HEIGHT);
  if (engine->texture == NULL) {
    result = EXIT_FAILURE;
    goto engine_init_end;
  }

  engine->pixels = malloc(GAME_WIDTH * GAME_HEIGHT * 4);
  if (engine->pixels == NULL) {
    result = EXIT_FAILURE;
    goto engine_init_end;
  }

  ENGINE_EVENT_TYPE = SDL_RegisterEvents(1);

  ABC_FIFO_create(&engine->fifo);
  engine->fifo.taskHandler = ENGINE_taskHandler;

engine_init_end:
  return result;
}



internal void
ENGINE_free(ENGINE* engine) {

  if (engine == NULL) {
    return;
  }

  ABC_FIFO_close(&engine->fifo);

  if (engine->fnMap.head != NULL) {
    MAP_free(&engine->fnMap);
  }

  if (engine->pixels != NULL) {
    free(engine->pixels);
  }

  if (engine->texture) {
    SDL_DestroyTexture(engine->texture);
  }

  if (engine->renderer != NULL) {
    SDL_DestroyRenderer(engine->renderer);
  }
  if (engine->window != NULL) {
    SDL_DestroyWindow(engine->window);
  }
}

internal void
ENGINE_pset(ENGINE* engine, int16_t x, int16_t y, uint32_t c) {
  // Draw pixel at (x,y)
  if ((c | (0xFF << 24)) == 0) {
    return;
  }
  if (0 <= x && x < GAME_WIDTH && 0 <= y && y < GAME_HEIGHT) {
    ((uint32_t*)(engine->pixels))[GAME_WIDTH * y + x] = c;
  }
}

internal void
ENGINE_print(ENGINE* engine, char* text, uint16_t x, uint16_t y, uint32_t c) {
  int fontWidth = 5;
  int fontHeight = 7;
  int cursor = 0;
  for (size_t pos = 0; pos < strlen(text); pos++) {
    char letter = text[pos];

    uint8_t* glyph = (uint8_t*)font[letter - 32];
    if (*glyph == '\n') {
      break;
    }
    for (int j = 0; j < fontHeight; j++) {
      for (int i = 0; i < fontWidth; i++) {
        uint8_t v = glyph[j * fontWidth + i];
        if (v != 0) {
          ENGINE_pset(engine, x + cursor + i, y + j, c);
        }
      }
    }
    cursor += 6;
  }
}

internal void
ENGINE_line_high(ENGINE* engine, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t c) {
  int16_t dx = x2 - x1;
  int16_t dy = y2 - y1;
  int16_t xi = 1;
  if (dx < 0) {
    xi = -1;
    dx = -dx;
  }
  int16_t p = 2 * dx - dy;

  int16_t y = y1;
  int16_t x = x1;
  while(y <= y2) {
    ENGINE_pset(engine, x, y, c);
    if (p > 0) {
      x += xi;
      p = p - 2 * dy;
    } else {
      p = p + 2 * dx;
    }
    y++;
  }
}

internal void
ENGINE_line_low(ENGINE* engine, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t c) {
  int16_t dx = x2 - x1;
  int16_t dy = y2 - y1;
  int16_t yi = 1;
  if (dy < 0) {
    yi = -1;
    dy = -dy;
  }
  int16_t p = 2 * dy - dx;

  int16_t y = y1;
  int16_t x = x1;
  while(x <= x2) {
    ENGINE_pset(engine, x, y, c);
    if (p > 0) {
      y += yi;
      p = p - 2 * dx;
    } else {
      p = p + 2 * dy;
    }
    x++;
  }
}

internal void
ENGINE_line(ENGINE* engine, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t c) {
  if (abs(y2 - y1) < abs(x2 - x1)) {
    if (x1 > x2) {
      ENGINE_line_low(engine, x2, y2, x1, y1, c);
    } else {
      ENGINE_line_low(engine, x1, y1, x2, y2, c);
    }
  } else {
    if (y1 > y2) {
      ENGINE_line_high(engine, x2, y2, x1, y1, c);
    } else {
      ENGINE_line_high(engine, x1, y1, x2, y2, c);
    }

  }

}

internal void
ENGINE_circle_filled(ENGINE* engine, int16_t x0, int16_t y0, int16_t r, uint32_t c) {
  int16_t x = 0;
  int16_t y = r;
  int16_t d = round(M_PI - (2*r));

  while (x <= y) {
    ENGINE_line(engine, x0 - x, y0 + y, x0 + x, y0 + y, c);
    ENGINE_line(engine, x0 - y, y0 + x, x0 + y, y0 + x, c);
    ENGINE_line(engine, x0 + x, y0 - y, x0 - x, y0 - y, c);
    ENGINE_line(engine, x0 - y, y0 - x, x0 + y, y0 - x, c);

    if (d < 0) {
      d = d + (M_PI * x) + (M_PI * 2);
    } else {
      d = d + (M_PI * (x - y)) + (M_PI * 3);
      y--;
    }
    x++;
  }
}

internal void
ENGINE_circle(ENGINE* engine, int16_t x0, int16_t y0, int16_t r, uint32_t c) {
  int16_t x = 0;
  int16_t y = r;
  int16_t d = round(M_PI - (2*r));

  while (x <= y) {
    ENGINE_pset(engine, x0 + x, y0 + y, c);
    ENGINE_pset(engine, x0 + y, y0 + x, c);
    ENGINE_pset(engine, x0 - y, y0 + x, c);
    ENGINE_pset(engine, x0 - x, y0 + y, c);

    ENGINE_pset(engine, x0 - x, y0 - y, c);
    ENGINE_pset(engine, x0 - y, y0 - x, c);
    ENGINE_pset(engine, x0 + y, y0 - x, c);
    ENGINE_pset(engine, x0 + x, y0 - y, c);

    if (d < 0) {
      d = d + (M_PI * x) + (M_PI * 2);
    } else {
      d = d + (M_PI * (x - y)) + (M_PI * 3);
      y--;
    }
    x++;
  }
}


internal void
ENGINE_rect(ENGINE* engine, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c) {
  ENGINE_line(engine, x, y, x, y+h-1, c);
  ENGINE_line(engine, x, y, x+w-1, y, c);
  ENGINE_line(engine, x, y+h-1, x+w-1, y+h-1, c);
  ENGINE_line(engine, x+w-1, y, x+w-1, y+h-1, c);
}

internal void
ENGINE_rectfill(ENGINE* engine, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c) {
  int16_t x1 = mid(0, x, GAME_WIDTH);
  int16_t y1 = mid(0, y, GAME_HEIGHT);
  int16_t x2 = mid(0, x + w, GAME_WIDTH);
  int16_t y2 = mid(0, y + h, GAME_HEIGHT);

  for (uint16_t j = y1; j < y2; j++) {
    for (uint16_t i = x1; i < x2; i++) {
      ENGINE_pset(engine, i, j, c);
    }
  }
}

internal void
ENGINE_storeKeyState(ENGINE* engine, SDL_Keycode keycode, uint8_t state) {
  if(keycode == SDLK_LEFT) {
    engine->keyboard.left.isPressed = (state == SDL_PRESSED);
  }
  if(keycode == SDLK_RIGHT) {
    engine->keyboard.right.isPressed = (state == SDL_PRESSED);
  }
  if(keycode == SDLK_UP) {
    engine->keyboard.up.isPressed = (state == SDL_PRESSED);
  }
  if(keycode == SDLK_DOWN) {
    engine->keyboard.down.isPressed = (state == SDL_PRESSED);
  }
  if(keycode == SDLK_SPACE) {
    engine->keyboard.space.isPressed = (state == SDL_PRESSED);
  }
}

internal KEY_STATE
ENGINE_getKeyState(ENGINE* engine, SDL_Keycode keycode) {
  if(keycode == SDLK_LEFT) {
    return engine->keyboard.left;
  }
  if(keycode == SDLK_RIGHT) {
    return engine->keyboard.right;
  }
  if(keycode == SDLK_UP) {
    return engine->keyboard.up;
  }
  if(keycode == SDLK_DOWN) {
    return engine->keyboard.down;
  }
  if(keycode == SDLK_SPACE) {
    return engine->keyboard.space;
  }
  KEY_STATE none;
  none.isPressed = false;
  return none;
}
