NAME 		= weberv

CXX 		= c++
CXXFLAGS 		= -Wall -Wextra -Werror -std=c++98

SRCDIR	= src
INCDIR	= include
OBJDIR	= obj

SRCS 		= $(SRCDIR)/main.cpp \
			  $(SRCDIR)/Config.cpp \
	   		  $(SRCDIR)/Utils.cpp

OBJS 		= $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

all: $(NAME)

$(NAME):$(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
