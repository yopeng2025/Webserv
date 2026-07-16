NAME 		= webserv

CXX 		= c++
CXXFLAGS 	= -Wall -Wextra -Werror -g -std=c++98 -MMD -MP

SRCDIR	= src
INCDIR	= include
OBJDIR	= obj

SRCS 		= $(SRCDIR)/main.cpp \
			$(SRCDIR)/Config.cpp \
			$(SRCDIR)/Server.cpp \
			$(SRCDIR)/Client.cpp \
			$(SRCDIR)/Request.cpp \
			$(SRCDIR)/Response.cpp \
			$(SRCDIR)/CGI.cpp \
			$(SRCDIR)/Router.cpp \
			$(SRCDIR)/Utils.cpp

OBJS 		= $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEP = $(OBJS:.o=.d)

all: $(NAME)

$(NAME):$(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)
	-@pkill -9 $(NAME) 2>/dev/null || true

fclean: clean
	rm -f $(NAME)

re: fclean 
	$(MAKE) all

-include $(DEP)

.PHONY: all clean fclean re
