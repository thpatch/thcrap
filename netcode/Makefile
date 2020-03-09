NAME = netcode

CXXFLAGS = -std=c++17 -g

LDFLAGS = -lcurl -ljansson -lpthread

SRCS = main.cpp \
	downloader.cpp \
	repo_discovery.cpp \
	server.cpp \
	file.cpp \
	http.cpp \

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	g++ $(OBJS) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(OBJS)
