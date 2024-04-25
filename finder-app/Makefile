CC := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,gcc)

SRC := writer.c 
TARGET := writer
OBJS := $(SRC:.c=.o)

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)


clean:
	$(RM) $(TARGET) $(OBJS)
