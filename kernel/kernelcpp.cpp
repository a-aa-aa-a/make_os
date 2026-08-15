#include <include/kernelapi.h>
#include <cstdint>
#define abc 0xFFFF800000000000
#define kernel_end 0x10000
extern "C" {
  void main();
}
void access_bit(uint64_t address,uint64_t bit,char value){
  uint64_t byte = bit / 8;
  bit %= 8;
  if(value == 0){
    *(char*)(address + byte) &= ~(1 << bit);
  }else{
    *(char*)(address + byte) |= (1 << bit);
  }
}
struct E820Entry {
    uint64_t base;   // 開始物理アドレス
    uint64_t length; // サイズ
    uint32_t type;   // 1 = usable RAM
    uint32_t gomi;   //gomi
};

struct memory_range {
  uint64_t start;
  uint64_t end;
};
memory_range used_address[] = {
  {0x0,kernel_end}
};
static E820Entry* const e820buffer = (E820Entry*)(abc + 0x9B00);

static char* const VGA_BASE = (char*)(abc + 0xB8000);
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
void hex_put(uint64_t n){
  int keta = 1;
  uint64_t b = 1;
  while(n / b >= 0x10){
    b *= 0x10;
    keta++;
  };
  for(int i = 0;i < keta;i++){
    hex_put1(n / b % 0x10);
    b /= 0x10;
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



uint64_t alignup(uint64_t a,uint64_t b){
  if(a % b == 0){
    return a;
  }else{
    return (a / b + 1) * b; 
  }
}

void kernelpanic(){
  put_text("kernelpanic");
  while(1);
}

#define aaa 0x100000
memory_range bitmap;

uint64_t bitmap_address;
void init_ram(){
  uint64_t ramlength  = 0;
  uint16_t a = *(uint16_t*)(abc + 0x9A00);
  for(int i = 0;i < a;i++){
    uint64_t base = e820buffer[i].base;
    uint64_t length = e820buffer[i].length;
    uint32_t type = e820buffer[i].type;

    if(type == 1){
      if(base + length > ramlength){
        ramlength = base + length;
      }

    }
  }
  
  int bit_amount = ramlength / 0x1000 / 8;
  bit_amount = alignup(bit_amount,0x1000);  
  for(int i = 0;i < bit_amount;i++){
    access_bit(abc + aaa,i,1);
  }

  for(int i = 0;i < a;i++){
    uint64_t base = e820buffer[i].base;
    uint64_t length = e820buffer[i].length;
    uint32_t type = e820buffer[i].type;
    if(type == 1){
      uint64_t start = base / 0x1000;
      uint64_t end = alignup((base + length)/ 0x1000,0x1000);
      for(int i = start;i < end;i++){
        access_bit(abc + aaa,i,0);
        
      }
    }
  }
  
  
  

}
uint64_t search_rsdp(){
  for (uint64_t addr = abc+ 0xE0000; addr < abc + 0x100000; addr += 16) {
    uint8_t* p = (uint8_t*)addr;

    if (p[0] == 'R' &&
        p[1] == 'S' &&
        p[2] == 'D' &&
        p[3] == ' ' &&
        p[4] == 'P' &&
        p[5] == 'T' &&
        p[6] == 'R' &&
        p[7] == ' ') {
        // RSDP発見
        return addr;
        
    };
  }
  return 0;
}
struct RSDP {
    char     signature[8];      // "RSD PTR "
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;

    // ACPI 2.0以降
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
};
void init_acpi(){
  uint64_t rsdp_addr = search_rsdp();
  RSDP* rsdp1 = (RSDP*)rsdp_addr;
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)rsdp_addr;
  for (int i = 0; i < 20; i++) {
    sum += p[i];
  }
  if(sum == 0){

  }else{
    for (int i = 20; i < 36; i++) {
      sum += p[i];
    }
  }

}
void main(){
  next_row();
  init_acpi();
  next_row();
  init_ram();
 
  next_row();

  next_row();
  hex_put(0xFF88);

}