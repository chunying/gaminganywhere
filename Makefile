TARGET = deps ga¬
  1 ¬
  2 .PHONY: $(TARGET)¬
  3 ¬
  4 all: $(TARGET)¬
  5 ¬
  6 deps:¬
  7 ▸ make -C deps.src¬
  8 ¬
  9 ga:¬
 10 ▸ make -C ga all¬
 11 ▸ make -C ga install