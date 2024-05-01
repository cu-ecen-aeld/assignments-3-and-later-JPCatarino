CC ?= $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,gcc)
CFLAGS ?= -g -Wall 
LDFLAGS ?= -pthread 

SRC ?= aesdsocket.c 
TARGET ?= aesdsocket 
OBJS ?= $(SRC:.c=.o)

USE_CHAR_DEVICE := -DUSE_AESD_CHAR_DEVICE=1

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(USE_CHAR_DEVICE) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)
