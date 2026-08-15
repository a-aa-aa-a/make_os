extern "C" {
  void main();
}





static char* const mem = (char*)0x7b00;
static char* const VGA_BASE = (char*)0xB8000;
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;
static int vga_pos = 8;

void put_vga(char c){
  VGA_BASE[vga_pos * 2]     = c;
  VGA_BASE[vga_pos * 2 + 1] = 0x0F;  // 白文字・黒背景
  vga_pos++;
  if (vga_pos >= VGA_COLS * VGA_ROWS) {
        vga_pos = 0;
    }
}
void put_text(const char* c){
  int i = 0;
  while(c[i] != '\0'){
    put_vga(c[i]);
    i++;
  }
}
void next_row(){
  vga_pos += VGA_COLS - (vga_pos % VGA_COLS);
}
void hex_put1(unsigned char b){
    if(0 <= b and b <= 9){
      put_vga('0' + b);
    }else if(10 <= b and b <= 15){
      put_vga('A' + b - 10);
    }
}
void hex_put2(char c[],int n){
  for(int i = 0;i < n;i++){
    if(i % 2 == 0){
      put_vga(' ');
    }
    unsigned char b = c[i];
    unsigned char b1 = b >> 4;
    hex_put1(b1);
    b = b & 0x0F;
    hex_put1(b);
  }
}


void dec_put(int n){
  int keta = 1;
  int b = 1;
  while(n / b >= 10){
    b *= 10;
    keta++;
  };
  for(int i = 0;i < keta;i++){
    put_vga('0' + n / b % 10);
    b /= 10;
  }
  return;
}

void main(){

  return;
}