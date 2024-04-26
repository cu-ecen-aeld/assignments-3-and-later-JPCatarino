CC := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,gcc)
CFLAGS := -g -Wall

SRC := aesdsocket.c 
TARGET := aesdsocket 
OBJS := $(SRC:.c=.o)

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)
