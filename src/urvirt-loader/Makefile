CFLAGS += -O -MMD -Wall -Wextra -I ../common
LDFLAGS = -static

OBJECTS = urvirt-loader.o
DEPENDS = $(OBJECTS:%.o=%.d)

.PHONY: all
all: urvirt-loader

urvirt-loader: $(OBJECTS)

-include $(DEPENDS)
