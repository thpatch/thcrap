NAME = netcode

CXXFLAGS  = -DUSE_HTTP_CURL -std=c++17 -g
CXXFLAGS += -Ithcrap_stuff

LDFLAGS = -lcurl -ljansson -lpthread

SRCS = main.cpp \
	downloader.cpp \
	update.cpp \
	repo_discovery.cpp \
	server.cpp \
	file.cpp \
	http_curl.cpp \
	download_url.cpp \
	random.cpp \
	thcrap_stuff/repo.cpp \

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	g++ $(OBJS) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(OBJS)
