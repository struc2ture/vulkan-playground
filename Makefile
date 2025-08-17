CC = clang++
CFLAGS = -g -I/opt/homebrew/include -I/usr/local/include -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends 
CFLAGS += -Wall -Werror -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lglfw -lvulkan

IMGUI_DIR = ../../other/imgui
IMGUI_SRC = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
IMGUI_SRC += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_vulkan.cpp

export VK_ICD_FILENAMES = /usr/local/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH = /usr/local/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH = /usr/local/lib:$DYLD_LIBRARY_PATH

build: bin/playground

bin/playground: src/main.cpp
	$(CC) $(CFLAGS) $(LFLAGS) $< $(IMGUI_SRC) -o $@

run: build
	lldb bin/playground -o run

clean:
	rm -rf bin
	mkdir bin

.PHONY: build run clean
