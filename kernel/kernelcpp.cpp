#include <include/kernelapi.h>
#include <cstdint>
#define directmap_addr_base 0xFFFF800000000000
#define kernel_end 0x100000
#define pml4 0x1000
#define MAX_BLOCK_DEVICES 32
extern "C" void (*isr_table[256])(void);
extern "C" void common_isr(uint64_t a);
uint64_t hpetcount = 0;
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

static struct idt_entry idt[256];

struct BlockDevice;

struct BlockDeviceOps {
    bool (*read)(
        BlockDevice* dev,
        uint64_t lba,
        uint32_t count,
        void* buffer
    );

    bool (*write)(
        BlockDevice* dev,
        uint64_t lba,
        uint32_t count,
        const void* buffer
    );
};
struct BlockDevice {
    uint64_t block_size;
    uint64_t block_count;

    BlockDeviceOps* ops;

    void* driver_data;
};
BlockDevice* block_devices[MAX_BLOCK_DEVICES];
uint32_t block_device_count = 0;

uint64_t mapping_end = 0x40000000;
uint8_t maxpaging = 0;
extern "C" {
  void main();
  void aaaa();
  void common_isr14(uint64_t error_code,
    uint64_t rip,
    uint64_t cr2);
}
uint64_t alloc_page();
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
#define used_addressrange 1
int memcmp(const void* a, const void* b, int n)
{
const unsigned char* p1 = (unsigned char*)a;
const unsigned char* p2 = (unsigned char*)b;
for (int i = 0; i < n; i++)
{
if (p1[i] != p2[i])
return p1[i] - p2[i];
}
return 0;
}


memory_range used_address[used_addressrange] = {
  {0x0,kernel_end}
};
static E820Entry* const e820buffer = (E820Entry*)(directmap_addr_base + 0x9B00);

static char* const VGA_BASE = (char*)(directmap_addr_base + 0xB8000);
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;
static int vga_pos = 8;

void next_row(){
  vga_pos += VGA_COLS - (vga_pos % VGA_COLS);
  if (vga_pos >= VGA_COLS * VGA_ROWS) {
      vga_pos = 0;
  }
}
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
    if(c[i] == '\n'){
      next_row();
      i++;
      continue;
    } 
    put_vga(c[i]);
    i++;
  }
}
void clearscreen(){
  for(int i = 0;i < 80;i++){
    for(int j = 0;j < 40;j++){
      put_vga(' ');
    }
  }
  vga_pos = 0;
}
void put_text(const char* c,int length){
  for(int i = 0;i < length;i++){
    put_vga(c[i]);
  }
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
static inline uint64_t read_cr2()
{
    uint64_t value;
    asm volatile (
        "mov %%cr2, %0"
        : "=r"(value)
    );
    return value;
}

void common_isr(uint64_t a){
  put_text("vector=");
  dec_put(a);
  
  if(a == 14){
    put_text("\n");
    hex_put(read_cr2());
    
  }
  
  
}
void kernelpanic(){
  put_text("kernelpanic");
  while(1);
}

#define aaa 0x100000
memory_range bitmap;
uint64_t bitmap_address = aaa;
void direct_map(uint64_t length){
  uint64_t* pml4_l = (uint64_t*)(directmap_addr_base + pml4); 
  uint64_t addr = directmap_addr_base + mapping_end;
  addr = addr >> 12;
  uint16_t pt_i = addr & 0b111111111;addr = addr >> 9;
  uint16_t pd_i = addr & 0b111111111;addr = addr >> 9;
  uint16_t pdpt_i = addr & 0b111111111;addr = addr >> 9;
  uint16_t pml4_i = addr & 0b111111111;
  if(maxpaging == 2){
    uint64_t phys = mapping_end & ~0x3FFFFFFFULL;
    uint64_t page_amount = alignup(length,0x40000000);page_amount /= 0x40000000;
    int maked_page = 0;
    for(int i = pml4_i;i < 512;i++){
      if( ( pml4_l[i] & 1) == 0){
        pml4_l[i] = 0x8000000000000003 | alloc_page();
      }
      uint64_t pdpt_addr = pml4_l[i]  & 0x0000FFFFFFFFF000ULL;
      uint64_t* pdpt_l = (uint64_t*)(directmap_addr_base + pdpt_addr);
      for(int j = pdpt_i;j < 512;j++){
        if( ( pdpt_l[j] & 1) == 0){
          pdpt_l[j] = phys | 0x8000000000000083ULL;
          
          maked_page++;
        }
        phys += 0x40000000ULL;
        if(maked_page >= page_amount){
          break;
        }
      }
      pdpt_i = 0;
      if(maked_page >= page_amount){
        break;
      }
    }
    mapping_end += page_amount * 0x40000000;
  }else if(maxpaging == 1){
    uint64_t phys = mapping_end & ~0x1FFFFFULL;
    uint64_t page_amount = alignup(length,0x200000);page_amount /= 0x200000;
    int maked_page = 0;
    for(int i = pml4_i;i < 512;i++){
      if( ( pml4_l[i] & 1) == 0){
        pml4_l[i] = 0x8000000000000003 | alloc_page();
      }
      uint64_t pdpt_addr = pml4_l[i]  & 0x0000FFFFFFFFF000ULL;
      uint64_t* pdpt_l = (uint64_t*)(directmap_addr_base + pdpt_addr);
      for(int j = pdpt_i;j < 512;j++){
        if( ( pdpt_l[j] & 1) == 0){
          pdpt_l[j] = alloc_page() | 0x8000000000000003ULL;
        }
        uint64_t pd_addr = pdpt_l[j]  & 0x0000FFFFFFFFF000ULL;
        uint64_t* pd_l = (uint64_t*)(directmap_addr_base + pd_addr);
        for(int k = pd_i;k < 512;k++){
          if( ( pd_l[k] & 1) == 0){
            pd_l[k] = phys | 0x8000000000000083ULL;
            maked_page++;
          }
          phys += 0x200000;
          if(maked_page >= page_amount){
            break;
          }
        }
        pd_i = 0;
        if(maked_page >= page_amount){
          break;
        }
      }
      pdpt_i = 0;
      if(maked_page >= page_amount){
        break;
      }
    }
    mapping_end += page_amount * 0x200000;
    
  }else{

    
  }

}

uint64_t ramlength  = 0;
void init_ram(){
  uint16_t a = *(uint16_t*)(directmap_addr_base + 0x9A00);
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
    access_bit(directmap_addr_base + bitmap_address,i,1);
  }
  if(ramlength >= mapping_end){
    direct_map(ramlength - mapping_end);
  }

  for(int i = 0;i < a;i++){
    uint64_t base = e820buffer[i].base;
    uint64_t length = e820buffer[i].length;
    uint32_t type = e820buffer[i].type;
    if(type == 1){
      uint64_t start = base / 0x1000;
      uint64_t end = alignup((base + length)/ 0x1000,0x1000);
      for(int i = start;i < end;i++){
        access_bit(directmap_addr_base + bitmap_address,i,0);
        
      }
    }
  }
  
  for(int i = 0;i < bit_amount / 0x1000;i++){
    access_bit(directmap_addr_base + bitmap_address + (0x100000 / 0x1000 / 8) ,i,1);
    
  }
  
  for(int i = 0;i < used_addressrange;i++){
    uint64_t start = used_address[i].start / 0x1000;
    uint64_t end = alignup(used_address[i].end,0x1000 ) / 0x1000;
    for(int j = start;j < end;j++){
      access_bit(directmap_addr_base + bitmap_address,j,1);
    }
    
  }

  

}
void page_zero(uint64_t phys)
{
    uint64_t *page =
        (uint64_t *)(directmap_addr_base + phys);

    for (int i = 0; i < 512; i++) {
        page[i] = 0;
    }
}
uint64_t alloc_page(){
  int n = alignup(ramlength,0x1000) / 0x1000;
  n = alignup(n,8);
  uint8_t* a = (uint8_t*)(directmap_addr_base + bitmap_address);
  for(int i = 0;i < n ;i++){  
    uint8_t ccc = (1 << (i % 8));
    uint8_t bbb = a[i / 8] & ccc;
    if(bbb == 0){
      access_bit(uint64_t(a),i,1);
      page_zero(i * 0x1000);
      return i * 0x1000;
    }
  }
  kernelpanic();
  return 0;
}
void free_page(uint64_t addr){
  uint8_t* a = (uint8_t*)(directmap_addr_base + bitmap_address);
  int addr2 = addr / 0x1000;
  access_bit(uint64_t(a),addr2,0);
}



bool mapping(
  uint64_t vir_addr,
  uint64_t addr,
  uint64_t flags
){
  flags |= 0x1;
  if( (vir_addr & 0xFFF) != 0){
    return 1;
  }
  if( (addr & 0xFFF) != 0){
    return 1;
  }
  uint64_t pml4_i,pdpt_i,pd_i,pt_i;
  vir_addr = (vir_addr >> 12);
  pt_i = 0b111111111; pt_i &= vir_addr;
  vir_addr = vir_addr >> 9;
  pd_i = 0b111111111; pd_i &= vir_addr;
  vir_addr = vir_addr >> 9;
  pdpt_i = 0b111111111; pdpt_i &= vir_addr;
  vir_addr = vir_addr >> 9;
  pml4_i = 0b111111111; pml4_i &= vir_addr;
  uint64_t pdpt,pd,pt;

  uint64_t* pml4_l = (uint64_t*)(directmap_addr_base + pml4);
  uint64_t* pdpt_l,*pd_l,*pt_l;
  if( (pml4_l[pml4_i] & 1 ) == 0){
    pdpt = alloc_page();
    pdpt_l = (uint64_t*)(directmap_addr_base + pdpt);
    for(int i = 0;i < 512;i++){
      pdpt_l[i] = 0;
    }
    pml4_l[pml4_i] = pdpt | flags;
  }else{
    pdpt = pml4_l[pml4_i] & 0x0000FFFFFFFFF000ULL ;
  }
  pdpt_l = (uint64_t*)(directmap_addr_base + pdpt);
  if( (pdpt_l[pdpt_i] & 1) == 0){
    pd = alloc_page();
    pd_l = (uint64_t*)(directmap_addr_base + pd);
    for(int i = 0;i < 512;i++){
      pd_l[i] = 0;
    }
    pdpt_l[pdpt_i] = pd | flags;
  }else{
    pd = pdpt_l[pdpt_i] & 0x0000FFFFFFFFF000ULL ;
  }
  pd_l = (uint64_t*)(directmap_addr_base + pd);
  if( (pd_l[pd_i] & 1) == 0){
    pt = alloc_page();
    pt_l = (uint64_t*)(directmap_addr_base + pt);
    for(int i = 0;i < 512;i++){
      pt_l[i] = 0;
    }
    pd_l[pd_i] = pt | flags;
  }else{
    pt = pd_l[pd_i] & 0x0000FFFFFFFFF000ULL ;
  }
  pt_l = (uint64_t*)(directmap_addr_base + pt);
  if( (pt_l[pt_i] & 1) == 0){
    pt_l[pt_i] = addr | flags;
    return 0;
  }else{
    return 1;
  }

}


uint64_t search_rsdp(){
  for (uint64_t addr = directmap_addr_base+ 0xE0000; addr < directmap_addr_base + 0x100000; addr += 16) {
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
void xsdt_init(uint64_t addr);
void rsdt_init(uint64_t addr);
void init_acpi(){
  uint64_t rsdp_addr = search_rsdp();
  RSDP* rsdp1 = (RSDP*)rsdp_addr;
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)rsdp_addr;
  int flag = 0;
  for (int i = 0; i < 20; i++) {
    sum += p[i];
  }
  if(sum == 0){
    flag = 1;
  }
  for (int i = 20; i < 36; i++) {
    sum += p[i];
  }
  if(sum == 0){
    flag = 2;
  }
  if(flag == 1){
    rsdt_init(rsdp1->rsdt_address);
  
  }else if(flag == 2){
    xsdt_init(rsdp1->xsdt_address);
  }else{
    kernelpanic();
  }
  

}
struct __attribute__((packed)) ACPISDTHeader {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};
struct __attribute__((packed)) BaseAddressaa {
        uint8_t  AddressSpaceId;
        uint8_t  RegisterBitWidth;
        uint8_t  RegisterBitOffset;
        uint8_t  AccessSize;
        uint64_t Address;
} ;
struct __attribute__((packed)) HPETTable{
    ACPISDTHeader Header;

    uint32_t EventTimerBlockId;
    BaseAddressaa BaseAddress;
  
    uint8_t  HpetNumber;
    uint16_t MinimumClockTick;
    uint8_t  PageProtection;
} ;

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static void idt_set_gate(
    int vector,
    uint64_t handler
)
{
    idt[vector].offset_low  = handler & 0xffff;
    idt[vector].selector    = 0x08;
    idt[vector].ist        = 0;
    idt[vector].type_attr  = 0x8e;
    idt[vector].offset_mid = (handler >> 16) & 0xffff;
    idt[vector].offset_high = (handler >> 32) & 0xffffffff;
    idt[vector].reserved   = 0;
}
uint64_t timer();
void idt_load(void)
{
    struct idtr idtr = {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)idt
    };

    asm volatile ("lidt %0" : : "m"(idtr));
}

static void dummy_handler(void)
{
    while (1) {
        asm volatile ("cli; hlt");
    }
}
int kari1 = 0;
int kari2 = 0;
void aaaa(){
  kari1 = vga_pos;
  vga_pos = 120;
  dec_put(kari2);
  vga_pos = kari1;
  kari2++;
  if(kari2 > 1 << 30){
    kari2 = 0;
  }
  *(volatile uint32_t *)(directmap_addr_base + 0xFEE00000 + 0x0B0) = 0;
}
extern "C" void abcd();
extern "C" void aisr13();
extern "C" void aisr14();
void idt_init(){
  for (int i = 0; i < 256; i++){
    if(i == 0x20){
      idt_set_gate(i, (uint64_t)abcd);
      continue;
    }
    if(i == 13){
      idt_set_gate(i, (uint64_t)aisr13);
      continue;
    }
    if(i == 14){
      idt_set_gate(i, (uint64_t)aisr14);
      continue;
    }
    idt_set_gate(i, (uint64_t)isr_table[i]);
  }
  idt_load();
}
struct __attribute__((packed)) madt {
    struct ACPISDTHeader header;

    uint32_t local_apic_address;
    uint32_t flags;

    uint8_t entries[];
};
struct __attribute__((packed)) mcfg_t{
    ACPISDTHeader header;
    uint64_t reserved;
};
struct __attribute__((packed)) mcfg_allocation_t{
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} ;
typedef struct {
    uint64_t physical_base;
    uint64_t virtual_base;

    uint16_t segment;
    uint8_t start_bus;
    uint8_t end_bus;
} pci_ecam_region_t;

#define PCI_MAX_ECAM_REGIONS 16

pci_ecam_region_t pci_ecam_regions[PCI_MAX_ECAM_REGIONS];
uint32_t pci_ecam_region_count = 0;
uint64_t karimcfg = 0;
uint64_t hpetaddr = 0;
void rsdt_init(uint64_t addr){
  put_text("ccc");
  if(addr >= mapping_end){
    direct_map(addr - mapping_end);
  }
  ACPISDTHeader* acpi = (ACPISDTHeader*)(directmap_addr_base + addr);
  uint8_t sum = 0;
  uint8_t* p = (uint8_t*)(directmap_addr_base + addr);
  if (p[0] == 'R' &&
        p[1] == 'S' &&
        p[2] == 'D' &&
        p[3] == 'T' ) {
        // RSDP発見      
  };
  for (int i = 0; i < acpi->length; i++) {
    sum += p[i];
  }
  uint32_t* acpi_table = (uint32_t*)(directmap_addr_base + addr + 36);
  for(int i = 0;i < (acpi->length - 36) / 4;i++){
    ACPISDTHeader* sdt = (ACPISDTHeader*)(directmap_addr_base + acpi_table[i]);
    put_text(sdt->signature,4); 
    if(memcmp(sdt->signature, "HPET", 4) == 0){
        HPETTable* hpet = (HPETTable*)sdt;
        put_text("hpetaaa");
        uint64_t mmio = hpet->BaseAddress.Address;
        hpetaddr = hpet->BaseAddress.Address;
       
        idt_init();

    }
    if(memcmp(sdt->signature, "APIC", 4) == 0){
        madt* apic = (madt*)sdt;
        uint64_t mmio = apic->local_apic_address;
        
    }
    if(memcmp(sdt->signature, "MCFG", 4) == 0){
      mcfg_t* mcfg = (mcfg_t*)sdt;    
      uint32_t count =
    (mcfg->header.length - sizeof(mcfg_t))
    / sizeof(mcfg_allocation_t);
    
    uint8_t *ptr =
    (uint8_t *)mcfg + sizeof(mcfg_t);

    for(int i = 0 ;i < count;i++){
      mcfg_allocation_t *alloc =
        (mcfg_allocation_t *)ptr;
  
      karimcfg = alloc->base_address;
      
      pci_ecam_region_t *region =
    &pci_ecam_regions[pci_ecam_region_count];
region->physical_base = alloc->base_address;
region->virtual_base = 0;
region->segment = alloc->segment_group;
region->start_bus = alloc->start_bus;
region->end_bus = alloc->end_bus;

pci_ecam_region_count++;
      ptr += sizeof(mcfg_allocation_t);

    }
    put_text("aiueo=");
    hex_put(pci_ecam_region_count);
    }
    put_text(" ");
  }
}
void xsdt_init(uint64_t addr){
  put_text("a");
  if(addr >= mapping_end){
    direct_map(addr - mapping_end);
  }
  
  if(addr + 36 >= mapping_end){
    direct_map(addr + 36 - mapping_end);
  }
  ACPISDTHeader* acpi = (ACPISDTHeader*)(directmap_addr_base + addr);
  uint8_t sum = 0;  
  uint8_t* p = (uint8_t*)(directmap_addr_base + addr);
  if (p[0] == 'X' &&
        p[1] == 'S' &&
        p[2] == 'D' &&
        p[3] == 'T' ) {
        // RSDP発見      
  }else {
    kernelpanic();
  };
  
  for (int i = 0; i < acpi->length; i++) {
    sum += p[i];
  }
  
  uint64_t* acpi_table = (uint64_t*)(directmap_addr_base + addr + 36);
  for(int i = 0;i < (acpi->length - 36) / 8;i++){
    ACPISDTHeader* sdt = (ACPISDTHeader*)(directmap_addr_base + acpi_table[i]);
    if(acpi_table[i] >= mapping_end){
      direct_map(addr - acpi_table[i]);
    }
    put_text(sdt->signature,4); 
    if(memcmp(sdt->signature, "HPET", 4) == 0){
        HPETTable* hpet = (HPETTable*)sdt;
        hpetaddr = hpet->BaseAddress.Address;
        idt_init();
    }
    if(memcmp(sdt->signature, "APIC", 4) == 0){
        madt* apic = (madt*)sdt;
        uint64_t mmio = apic->local_apic_address;
    }
    if(memcmp(sdt->signature, "MCFG", 4) == 0){
      mcfg_t* mcfg = (mcfg_t*)sdt;    
      uint32_t count =
    (mcfg->header.length - sizeof(mcfg_t))
    / sizeof(mcfg_allocation_t);
    
    uint8_t *ptr =
    (uint8_t *)mcfg + sizeof(mcfg_t);

    for(int i = 0 ;i < count;i++){
      mcfg_allocation_t *alloc =
        (mcfg_allocation_t *)ptr;
    
      karimcfg = alloc->base_address;
          
      pci_ecam_region_t *region =
    &pci_ecam_regions[pci_ecam_region_count];
region->physical_base = alloc->base_address;
region->virtual_base = 0;
region->segment = alloc->segment_group;
region->start_bus = alloc->start_bus;
region->end_bus = alloc->end_bus;

pci_ecam_region_count++;
      ptr += sizeof(mcfg_allocation_t);

    }
    put_text("aiueo=");
    hex_put(pci_ecam_region_count);
    }
    put_text(" ");
  }
}

extern "C" void aiueo();

uint64_t rdmsr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}
void common_isr14(uint64_t error_code,
    uint64_t rip,
    uint64_t cr2){
  put_text("\n");
  put_text("errorcode=");
  hex_put(error_code);
  put_text("\n");
  put_text("\n");  
  put_text("rip=");
  hex_put(rip);
  put_text("\n");
  put_text("\n");
  put_text("cr2=");
  hex_put(cr2);
  put_text("\n");
}

void init_maxpaging(){
  uint64_t* pml4_l = (uint64_t*)(directmap_addr_base + pml4);
  uint64_t* pdpt_l = (uint64_t*)(directmap_addr_base + (pml4_l[256] & 0x0000FFFFFFFFF000ULL) );
  if( (pdpt_l[0] & 0x80) == 0x80){
    maxpaging = 2;
    return;
  }
  uint64_t* pd_l = (uint64_t*)(directmap_addr_base + (pdpt_l[0] & 0x0000FFFFFFFFF000ULL) );

  if( (pd_l[0] & 0x80) == 0x80){
    maxpaging = 1;
    return;
  }
  return;
}


void pci_init(){

}
uint64_t pci_ecam_addr(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint16_t offset
)
{
    return directmap_addr_base +  karimcfg
         + ((uint64_t)bus << 20)
         + ((uint64_t)device << 15)
         + ((uint64_t)function << 12)
         + offset;
}
uint64_t ehciaddr = 0;
uint64_t ehcidevice = 0;
uint64_t ehcifunction = 0;
void pci_scan(){
  uint32_t *kari2 = (uint32_t*)(directmap_addr_base + karimcfg);
  hex_put(kari2[2]);
  put_text(" \n");

  for (int device = 0; device < 32; device++) {
    uint64_t addr0 =  pci_ecam_addr(0, device, 0, 0);
    uint16_t vendor0 = *(volatile uint16_t*)(addr0 + 0x00);
    if (vendor0 == 0xFFFF) continue;
    for (int function = 0;function < 8;function++) {
                uint64_t addr = pci_ecam_addr(
                    0, device, function, 0
                );
                uint16_t vendor =
                    *(volatile uint16_t*)(addr + 0x00);
                if (vendor == 0xFFFF)
                    continue;
                uint16_t device_id =
                    *(volatile uint16_t*)(addr + 0x02);
                uint32_t classreg =
                    *(volatile uint32_t*)(addr + 0x08);
                uint8_t progif =
                    (classreg >> 8) & 0xFF;
                uint8_t subclass =
                    (classreg >> 16) & 0xFF;
                uint8_t class_code =
                    (classreg >> 24) & 0xFF;
                if (class_code != 0x0C || subclass != 0x03)continue;
                if(progif == 0x20){
                 
                  ehciaddr = *(uint32_t*)pci_ecam_addr(0,device,function,0x10);
                  ehcidevice = device;
                  ehcifunction = function;      

      
                }
      }
  }
        // BAR0～BAR5の取得など、次の初期化処理へ
    


}
enum {
    EHCI_USBCMD       = 0x00 / 4,
    EHCI_USBSTS       = 0x04 / 4,
    EHCI_USBINTR      = 0x08 / 4,
    
    EHCI_ASYNCLISTADDR = 0x18 / 4,
    EHCI_CONFIGFLAG = 0x40 / 4,
    EHCI_PORTSC1    = 0x44 / 4
};
#define USBCMD_RS       (1u << 0)
#define USBCMD_HCRESET  (1u << 1)
#define USBCMD_ASE      (1u << 5)

#define USBSTS_HCHALTED (1u << 12)
#define USBSTS_ASS      (1u << 15)

#define PORTSC_CCS   (1u << 0)  // Current Connect Status
#define PORTSC_CSC   (1u << 1)  // Connect Status Change (RWC)
#define PORTSC_PED   (1u << 2)  // Port Enable
#define PORTSC_PE    (1u << 2)
#define PORTSC_PEC   (1u << 3)  // Port Enable Change (RWC)
#define PORTSC_OCC   (1u << 5)  // Over-current Change (RWC)
#define PORTSC_PR    (1u << 8)  // Port Reset
#define PORTSC_LS_MASK (3u << 10) // Line Status
#define PORTSC_LS_K    (1u << 10) // K-state = Low-speed device
#define PORTSC_PP    (1u << 12) // Port Power
#define PORTSC_PO    (1u << 13) // Port Owner (1 = コンパニオンコントローラへ)

// 書き込むと1がクリアされる変化通知ビット
#define PORTSC_RWC   (PORTSC_CSC | PORTSC_PEC | PORTSC_OCC)

#define HCSPARAMS_PPC (1u << 4) // Port Power Control

#ifndef EHCI_CONFIGFLAG
#define EHCI_CONFIGFLAG (0x40 / 4) // op[0x10]
#endif

#define EHCI_LEGSUP_OS_OWNED   (1u << 24)
#define EHCI_LEGSUP_BIOS_OWNED (1u << 16)

// ehci_reset_port の戻り値
#define PORT_NONE     0  // 未接続 / 失敗
#define PORT_ENABLED  1  // High-speed でEHCIが有効化した
#define PORT_COMPANION 2 // LS/FS デバイス。コンパニオンへ移譲した

bool wait_reg(
    volatile uint32_t *reg,
    uint32_t mask,
    uint32_t expected,
    uint64_t timeout_ns
){
    uint64_t timerstart = timer();
    while (1) {
        uint32_t value = *reg;

        if ((value & mask) == expected) {
            return true;
        }

        uint64_t time = timer() - timerstart;
        time *= hpetcount / 1000000;

        if (time >= timeout_ns) {
            return false;
        }

        __asm__ volatile("pause");
    }
}

void wait_ns(uint64_t time1){
  uint64_t timerstart = timer();

  while(1){
      uint64_t time = timer() - timerstart;
      time *= hpetcount / 1000000;
      if (time >= time1) {
          return;
      }
      __asm__ volatile("pause");
  }
}

static inline void wait_ms(uint64_t ms){
    wait_ns(ms * 1000000ULL);
}

// BIOS(SMM)からEHCIの所有権を奪う。
// EECP(HCCPARAMS[15:8])はPCIコンフィグ空間上のExtended Capabilityの先頭オフセット。
// 0x68固定ではなく、リンクをたどってLegacy Support(ID=1)を探す。
static void ehci_bios_handoff(uint32_t eecp)
{
    if (eecp < 0x40) return; // Extended Capabilityなし

    for (int n = 0; n < 16 && eecp != 0; n++) {
        volatile uint32_t *cap =
            (volatile uint32_t *)pci_ecam_addr(0, ehcidevice, ehcifunction, eecp);
        uint32_t v = *cap;
        uint8_t id   = v & 0xFF;
        uint8_t next = (v >> 8) & 0xFF;

        if (id == 1) {
            volatile uint8_t *bios_sem =
                (volatile uint8_t *)pci_ecam_addr(0, ehcidevice, ehcifunction, eecp + 2);
            volatile uint8_t *os_sem =
                (volatile uint8_t *)pci_ecam_addr(0, ehcidevice, ehcifunction, eecp + 3);

            *os_sem = 1; // OS Owned Semaphore をセット

            // BIOSが手放すのを最大1秒待つ
            uint64_t start = timer();
            while (*bios_sem & 1) {
                uint64_t t = (timer() - start) * (hpetcount / 1000000);
                if (t >= 1000000000ULL) {
                    put_text("BIOS handoff timeout\n");
                    break;
                }
                __asm__ volatile("pause");
            }

            // SMI生成を止め、ステータス(RWC)をクリア
            volatile uint32_t *ctlsts =
                (volatile uint32_t *)pci_ecam_addr(0, ehcidevice, ehcifunction, eecp + 4);
            *ctlsts = 0xE0000000u;
            return;
        }

        eecp = next;
    }
}

bool ehci_stop_and_reset(volatile uint32_t *op)
{
    op[EHCI_USBINTR] = 0;

    put_text("stop\n");

    op[EHCI_USBCMD] &= ~USBCMD_RS;

    if (!wait_reg(
            &op[EHCI_USBSTS],
            USBSTS_HCHALTED,
            USBSTS_HCHALTED,
            100000000ULL)) {
        put_text("halt timeout\n");
        return false;
    }

    put_text("halted\n");

    op[EHCI_USBCMD] |= USBCMD_HCRESET;

    if (!wait_reg(
            &op[EHCI_USBCMD],
            USBCMD_HCRESET,
            0,
            100000000ULL)) {
        put_text("reset timeout\n");
        return false;
    }

    put_text("reset done\n");
    return true;
}

typedef struct __attribute__((packed, aligned(32))) {
    uint32_t horizontal_link;
    uint32_t endpoint_char;
    uint32_t endpoint_cap;
    uint32_t current_qtd;
    uint32_t next_qtd;
    uint32_t alt_next_qtd;
    uint32_t token;
    uint32_t buffer[5];
} ehci_qh_t;


bool ehci_init_qh(ehci_qh_t *qh, uint64_t qh_phys)
{
    if (qh == nullptr)
        return false;

    // EHCIのQHは32バイト境界に配置する
    if ((qh_phys & 0x1F) != 0)
        return false;

    // 32ビット物理アドレスで扱う
    if (qh_phys > 0xFFFFFFFFULL)
        return false;

    // QH全体をゼロ初期化
    uint32_t *p = reinterpret_cast<uint32_t *>(qh);

    for (uint32_t i = 0; i < sizeof(ehci_qh_t) / sizeof(uint32_t); i++) {
        p[i] = 0;
    }

    // 自分自身を指すリンク（QHタイプ = 01b）
    qh->horizontal_link = static_cast<uint32_t>(qh_phys) | 0x02;

    // Head of Reclamation List
    qh->endpoint_char = (1u << 15);

    // 何も転送しないダミーQH
    qh->endpoint_cap = (1u << 30); // Mult = 1
    qh->next_qtd = 1;              // Tビット
    qh->alt_next_qtd = 1;
    qh->token = (1u << 6);         // Halted

    return true;
}
bool ehci_init_qh(
    ehci_qh_t *qh,
    uint64_t qh_phys,
    uint8_t device_addr,
    uint8_t endpoint,
    uint8_t speed,
    uint16_t max_packet_size
)
{
    if (qh == nullptr)
        return false;

    // EHCI QHは32バイト境界
    if ((qh_phys & 0x1F) != 0)
        return false;

    // EHCIのQHリンクは32bit物理アドレス
    if (qh_phys > 0xFFFFFFFFULL)
        return false;

    // Device Addressは7bit
    if (device_addr > 127)
        return false;

    // Endpoint Numberは4bit
    if (endpoint > 15)
        return false;

    // MPSは11bit
    if (max_packet_size == 0 || max_packet_size > 2047)
        return false;

    // QH全体をゼロ初期化
    uint32_t *p = reinterpret_cast<uint32_t *>(qh);

    for (uint32_t i = 0;
         i < sizeof(ehci_qh_t) / sizeof(uint32_t);
         i++) {
        p[i] = 0;
    }

    // 自分自身を指すリンク
    // 01b = QH
    qh->horizontal_link =
        static_cast<uint32_t>(qh_phys) | 0x02;

    /*
     * endpoint_char
     *
     * bits  6:0  Device Address
     * bits 11:8  Endpoint Number
     * bit  14    DTC
     * bit  15    Head of Reclamation List
     * bits 13:12  Speed
     * bits 26:16  Maximum Packet Length
     */

    uint32_t ep_char = 0;

    // Device Address
    ep_char |= ((uint32_t)device_addr & 0x7F);

    // Endpoint Number
    ep_char |= ((uint32_t)endpoint & 0x0F) << 8;

    // DTC
    ep_char |= (1u << 14);

    // Head of Reclamation List
    // 必要なQHだけで使用するので、通常のデバイスQHでは0
    // ep_char |= (1u << 15);

    // Speed
    //
    // EHCI:
    // 00 = Full Speed
    // 01 = Low Speed
    // 10 = High Speed
    //
    ep_char |= ((uint32_t)speed & 0x03) << 12;

    // Maximum Packet Length
    ep_char |= ((uint32_t)max_packet_size & 0x7FF) << 16;

    qh->endpoint_char = ep_char;

    // Mult = 1
    qh->endpoint_cap = (1u << 30);

    // qTDなし
    qh->current_qtd = 0;
    qh->next_qtd = 1;       // T bit
    qh->alt_next_qtd = 1;   // T bit

    // Halted
    qh->token = (1u << 6);

    return true;
}
bool ehci_create_bulk_qh(
    ehci_qh_t **qh,
    uint64_t *qh_phys,
    uint8_t addr,
    uint8_t endpoint,
    uint8_t speed,
    uint16_t mps
)
{
    *qh_phys = alloc_page();

    if (*qh_phys == 0)
        return false;

    *qh = (ehci_qh_t *)(directmap_addr_base + *qh_phys);

    return ehci_init_qh(
        *qh,
        *qh_phys,
        addr,
        endpoint,
        speed,
        mps
    );
}
bool ehci_start_async(
    volatile uint32_t *op,
    uint64_t qh_phys
) {
    if ((qh_phys & 0x1F) != 0)
        return false;

    if (qh_phys > 0xFFFFFFFFULL)
        return false;

    if ((op[EHCI_USBSTS] & USBSTS_HCHALTED) == 0) {
        put_text("EHCI was not halted\n");
        return false;
    }

    op[EHCI_USBINTR] = 0;

    // QHの物理アドレス
    op[EHCI_ASYNCLISTADDR] = (uint32_t)qh_phys;

    // 非同期スケジュールとコントローラーを起動
    uint32_t cmd = op[EHCI_USBCMD];
    cmd |= USBCMD_ASE | USBCMD_RS;
    op[EHCI_USBCMD] = cmd;

    if (!wait_reg(&op[EHCI_USBSTS], USBSTS_HCHALTED, 0, 100000000ULL)) {
        put_text("HCHalted not cleared\n");
        return false;
    }
    put_text("HCHalted cleared\n");

    if (!wait_reg(&op[EHCI_USBSTS], USBSTS_ASS, USBSTS_ASS, 100000000ULL)) {
        put_text("Async timeout CMD=");
        hex_put(op[EHCI_USBCMD]);
        put_text(" STS=");
        hex_put(op[EHCI_USBSTS]);
        put_text("\n");
        return false;
    }

    put_text("Async enabled\n");
    return true;
}

void ehci_dump_ports(
    volatile uint32_t *op,
    uint32_t port_count
) {
    for (uint32_t i = 0; i < port_count; i++) {
        uint32_t portsc = op[EHCI_PORTSC1 + i];

        put_text("EHCI PORT ");
        hex_put(i + 1);
        put_text(" STATUS ");
        hex_put(portsc);
        put_text("\n");
    }
}

static inline void ehci_port_write(
    volatile uint32_t *op,
    uint32_t port_index,
    uint32_t value
) {
    op[EHCI_PORTSC1 + port_index] = value;
}

static inline uint32_t ehci_port_read(
    volatile uint32_t *op,
    uint32_t port_index
) {
    return op[EHCI_PORTSC1 + port_index];
}

// 全ポートに電源を入れる。
// HCRESET後はPortPowerが落ちていることがあり、PPC=1の場合はソフトで入れる必要がある。
static void ehci_power_ports(
    volatile uint32_t *op,
    uint32_t hcsparams,
    uint32_t port_count
) {
    if (hcsparams & HCSPARAMS_PPC) {
        for (uint32_t i = 0; i < port_count; i++) {
            uint32_t portsc = ehci_port_read(op, i);
            if ((portsc & PORTSC_PP) == 0) {
                ehci_port_write(op, i, (portsc & ~PORTSC_RWC) | PORTSC_PP);
            }
        }
    }

    // 電源安定 + 接続デバウンス待ち
    wait_ms(100);
}

// LS/FSデバイスなどEHCIが扱えないポートをコンパニオンコントローラへ移譲する
static void ehci_release_port(volatile uint32_t *op, uint32_t port_index)
{
    uint32_t portsc = ehci_port_read(op, port_index);
    ehci_port_write(op, port_index, (portsc & ~PORTSC_RWC) | PORTSC_PO);
}

// 戻り値: PORT_NONE / PORT_ENABLED / PORT_COMPANION
int ehci_reset_port(
    volatile uint32_t *op,
    uint32_t port_index
) {
    uint32_t portsc = ehci_port_read(op, port_index);

    // 接続されていなければ何もしない
    if ((portsc & PORTSC_CCS) == 0) {
        return PORT_NONE;
    }

    // K-stateならLow-speedデバイス。EHCIはリセットせずコンパニオンへ渡す
    if ((portsc & PORTSC_LS_MASK) == PORTSC_LS_K) {
        ehci_release_port(op, port_index);
        return PORT_COMPANION;
    }

    // リセット開始 (PEをクリアし、変化ビットには0を書いて触らない)
    uint32_t value = portsc & ~(PORTSC_PE | PORTSC_RWC);
    value |= PORTSC_PR;
    ehci_port_write(op, port_index, value);

    // USB仕様: リセットは50ms以上維持する (以前は5msだった)
    wait_ms(50);

    // リセット解除
    portsc = ehci_port_read(op, port_index);
    value = portsc & ~(PORTSC_PR | PORTSC_RWC);
    ehci_port_write(op, port_index, value);

    // コントローラがPRを0に戻すのを待つ (仕様上は2ms以内)
    if (!wait_reg(&op[EHCI_PORTSC1 + port_index], PORTSC_PR, 0, 100000000ULL)) {
        put_text("port reset stuck\n");
        return PORT_NONE;
    }

    // リセット回復時間 (TRSTRCY >= 10ms)
    wait_ms(10);

    portsc = ehci_port_read(op, port_index);

    if ((portsc & PORTSC_CCS) == 0) {
        return PORT_NONE; // リセット中に抜かれた
    }

    if ((portsc & PORTSC_PE) == 0) {
        // High-speedのチャープに応答しなかった = Full-speedデバイス
        ehci_release_port(op, port_index);
        return PORT_COMPANION;
    }

    // 変化ビット(CSC/PEC/OCC)に1を書いてクリア
    ehci_port_write(op, port_index, portsc | PORTSC_RWC);

    return PORT_ENABLED;
}

// 全ポートを初期化し、EHCIで有効になったポート数を返す
uint32_t ehci_init_ports(volatile uint32_t *op, uint32_t port_count)
{
    uint32_t enabled = 0;

    for (uint32_t i = 0; i < port_count; i++) {
        int r = ehci_reset_port(op, i);
        put_text("PORT ");
        hex_put(i + 1);
        if (r == PORT_ENABLED) {
            put_text(" HS enabled");
            enabled++;
        } else if (r == PORT_COMPANION) {
            put_text(" released to companion");
        } else {
            put_text(" none");
        }
        put_text(" STATUS=");
        hex_put(ehci_port_read(op, i));
        put_text("\n");
    }

    return enabled;
}

// ehci.cpp の定義(ehci_qh_t, wait_reg, wait_ms, timer など)の後ろに追加する。
// 前回の ehci_control.cpp の中身は、このファイルで丸ごと置き換える。
//
// 内容:
//   ehci_control_transfer()  汎用コントロール転送 (SETUP → DATA → STATUS)
//   usb_enumerate_msc()      列挙: アドレス設定 → 記述子取得 → Mass Storage検出 → SET_CONFIGURATION

// ---- qTD ----
typedef struct __attribute__((packed, aligned(32))) {
    uint32_t next_qtd;      // 次のqTDの物理アドレス (bit0 = T: 終端)
    uint32_t alt_next_qtd;  // ショートパケット時の次
    uint32_t token;
    uint32_t buffer[5];     // 4KBページごとの物理アドレス (buffer[0]は下位12bitがオフセット)
} ehci_qtd_t;

#define QTD_PID_OUT    0
#define QTD_PID_IN     1
#define QTD_PID_SETUP  2

#define QTD_ACTIVE     (1u << 7)
#define QTD_ERRMASK    0x78u   // Halted | DataBufferError | Babble | XactErr

#define USBCMD_IAAD    (1u << 6)  // Interrupt on Async Advance Doorbell
#define USBSTS_IAA     (1u << 5)  // Interrupt on Async Advance

static inline uint32_t qtd_token(uint32_t pid, uint32_t len, uint32_t toggle) {
    return (toggle << 31) | (len << 16) | (3u << 10) /*CERR=3*/ |
           (pid << 8) | QTD_ACTIVE;
}
bool ehci_init_qtd(
    ehci_qtd_t *qtd,
    uint64_t qtd_phys
)
{
    if (qtd == nullptr)
        return false;

    if ((qtd_phys & 0x1F) != 0)
        return false;

    if (qtd_phys > 0xFFFFFFFFULL)
        return false;

    uint32_t *p =
        (uint32_t *)qtd;

    for (uint32_t i = 0;
         i < sizeof(ehci_qtd_t) / sizeof(uint32_t);
         i++) {
        p[i] = 0;
    }

    // 次のqTDなし
    qtd->next_qtd = 1;

    // Alternate Nextもなし
    qtd->alt_next_qtd = 1;

    // Halted
    qtd->token = (1u << 6);

    return true;
}
typedef struct __attribute__((packed)) {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_transfer_length;

    uint8_t flags;
    uint8_t lun;
    uint8_t cb_length;

    uint8_t cb[16];

} usb_msc_cbw_t;

void msc_make_read10_cbw(
    usb_msc_cbw_t *cbw,
    uint32_t tag,
    uint32_t lba,
    uint16_t blocks
)
{
    uint8_t *p =
        (uint8_t *)cbw;

    for (uint32_t i = 0;
         i < sizeof(usb_msc_cbw_t);
         i++) {
        p[i] = 0;
    }

    cbw->signature = 0x43425355;

    cbw->tag = tag;

    cbw->data_transfer_length =
        (uint32_t)blocks * 512;

    // Device → Host
    cbw->flags = 0x80;

    cbw->lun = 0;

    cbw->cb_length = 10;

    // SCSI READ(10)
    cbw->cb[0] = 0x28;

    // LBAはbig endian
    cbw->cb[2] = (lba >> 24) & 0xFF;
    cbw->cb[3] = (lba >> 16) & 0xFF;
    cbw->cb[4] = (lba >> 8) & 0xFF;
    cbw->cb[5] = lba & 0xFF;

    // Transfer Length
    cbw->cb[7] = (blocks >> 8) & 0xFF;
    cbw->cb[8] = blocks & 0xFF;
}
#define MSC_CSW_PASSED  0x00
#define MSC_CSW_FAILED  0x01
#define MSC_CSW_PHASE   0x02
typedef struct __attribute__((packed)) {
    uint32_t signature = 0x53425355;
    uint32_t tag;
    uint32_t data_residue;
    uint8_t  status;
} usb_msc_csw_t;



// ---- USB SETUPパケット (8バイト) ----
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

static usb_setup_t mk_setup(uint8_t type, uint8_t req, uint16_t value,
                            uint16_t index, uint16_t length) {
    usb_setup_t s;
    s.bmRequestType = type;
    s.bRequest = req;
    s.wValue = value;
    s.wIndex = index;
    s.wLength = length;
    return s;
}

static void zero_bytes(void *p, uint32_t n) {
    volatile uint8_t *b = (volatile uint8_t *)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

// 制御転送用のメモリは最初の1回だけ確保して使い回す (QHは毎回リストから外すので安全)
static uint64_t ctrl_page = 0;       // QH + qTD x3 + SETUPパケット
static uint64_t ctrl_data_page = 0;  // データ用 (4KB, ページ境界なので跨がない)

// 汎用コントロール転送
//   addr   : デバイスアドレス (未設定なら0)
//   mps    : EP0の最大パケットサイズ (HSなら64)
//   req    : SETUPパケット (wLength がデータ長)
//   data   : IN なら受信先、OUT なら送信元 (wLength==0 なら nullptr可)
//   dir_in : DATAフェーズの向き (true: デバイス→ホスト)
// wLength は4096まで。
bool ehci_control_transfer(
    volatile uint32_t *op,
    ehci_qh_t *head_qh,
    uint64_t head_qh_phys,
    uint8_t addr,
    uint16_t mps,
    const usb_setup_t *req,
    void *data,
    bool dir_in
) {
    uint16_t len = req->wLength;
    if (len > 4096) return false;

    if (ctrl_page == 0) {
        ctrl_page = alloc_page();
        ctrl_data_page = alloc_page();
    }

    // レイアウト
    //   +0x000 QH   +0x040 qTD0(SETUP)  +0x080 qTD1(DATA)  +0x0C0 qTD2(STATUS)
    //   +0x100 SETUPパケット
    uint8_t *v = (uint8_t *)(directmap_addr_base + ctrl_page);
    zero_bytes(v, 0x200);

    volatile ehci_qh_t  *qh   = (volatile ehci_qh_t  *)(v + 0x000);
    volatile ehci_qtd_t *qtd0 = (volatile ehci_qtd_t *)(v + 0x040);
    volatile ehci_qtd_t *qtd1 = (volatile ehci_qtd_t *)(v + 0x080);
    volatile ehci_qtd_t *qtd2 = (volatile ehci_qtd_t *)(v + 0x0C0);
    volatile uint8_t *setup_buf = v + 0x100;
    volatile uint8_t *dbuf = (volatile uint8_t *)(directmap_addr_base + ctrl_data_page);

    uint32_t qh_p    = (uint32_t)(ctrl_page + 0x000);
    uint32_t qtd0_p  = (uint32_t)(ctrl_page + 0x040);
    uint32_t qtd1_p  = (uint32_t)(ctrl_page + 0x080);
    uint32_t qtd2_p  = (uint32_t)(ctrl_page + 0x0C0);
    uint32_t setup_p = (uint32_t)(ctrl_page + 0x100);

    // SETUPパケットをコピー
    const uint8_t *rq = (const uint8_t *)req;
    for (int i = 0; i < 8; i++) setup_buf[i] = rq[i];

    // OUTデータは送信前にバッファへ
    if (len > 0 && !dir_in) {
        const uint8_t *src = (const uint8_t *)data;
        for (uint16_t i = 0; i < len; i++) dbuf[i] = src[i];
    }

    // ① qTDチェーン: SETUP → (DATA) → STATUS
    qtd0->alt_next_qtd = 1;
    qtd0->token = qtd_token(QTD_PID_SETUP, 8, 0);
    qtd0->buffer[0] = setup_p;

    if (len > 0) {
        qtd0->next_qtd = qtd1_p;

        qtd1->next_qtd = qtd2_p;
        qtd1->alt_next_qtd = qtd2_p; // ショートパケットでもSTATUSへ進める
        qtd1->token = qtd_token(dir_in ? QTD_PID_IN : QTD_PID_OUT, len, 1);
        qtd1->buffer[0] = (uint32_t)ctrl_data_page;
    } else {
        qtd0->next_qtd = qtd2_p;
    }

    // STATUSはDATAと逆向き (DATAなしならIN)。トグルは常に1
    qtd2->next_qtd = 1;
    qtd2->alt_next_qtd = 1;
    qtd2->token = qtd_token((len > 0 && dir_in) ? QTD_PID_OUT : QTD_PID_IN, 0, 1)
                  | (1u << 15); // IOC
    qtd2->buffer[0] = 0;

    // ② QH
    qh->horizontal_link = (uint32_t)head_qh_phys | 0x02;
    qh->endpoint_char   = (uint32_t)addr
                        | (0u << 8)                 // endpoint 0
                        | (2u << 12)                // High-speed
                        | (1u << 14)                // DTC: トグルはqTDから
                        | ((uint32_t)mps << 16);
    qh->endpoint_cap    = (1u << 30);               // Mult = 1
    qh->next_qtd        = qtd0_p;
    qh->alt_next_qtd    = 1;
    qh->token           = 0;

    __sync_synchronize();

    // ③ 非同期リストへ挿入
    qh->horizontal_link = head_qh->horizontal_link;
    __sync_synchronize();
    head_qh->horizontal_link = qh_p | 0x02;

    // ④ 完了待ち
    bool ok = false;
    uint64_t start = timer();
    while (1) {
        uint32_t t0 = qtd0->token, t1 = qtd1->token, t2 = qtd2->token;

        uint32_t err = t0 | t2;
        if (len > 0) err |= t1;
        if (err & QTD_ERRMASK) {
            put_text("qTD error STS="); hex_put(op[EHCI_USBSTS]);
            put_text(" QHTOK=");         hex_put(qh->token);
            put_text(" T0=");            hex_put(t0);
            put_text(" T1=");          hex_put(t1);
            put_text(" T2=");          hex_put(t2);
            put_text("\n");
            break;
        }
        if ((t2 & QTD_ACTIVE) == 0) { ok = true; break; }

        uint64_t el = (timer() - start) * (hpetcount / 1000000);
        if (el >= 500000000ULL) {
            put_text("control timeout T0="); hex_put(t0);
            put_text(" T1=");                hex_put(t1);
            put_text(" T2=");                hex_put(t2);
            put_text("\n");
            break;
        }
        __asm__ volatile("pause");
    }

    // ⑤ リストから外してドアベル
    head_qh->horizontal_link = (uint32_t)head_qh_phys | 0x02;
    __sync_synchronize();
    op[EHCI_USBCMD] |= USBCMD_IAAD;
    wait_reg(&op[EHCI_USBSTS], USBSTS_IAA, USBSTS_IAA, 100000000ULL);
    op[EHCI_USBSTS] = USBSTS_IAA;  // RWC

    // ⑥ INデータを呼び出し元へ
    if (ok && len > 0 && dir_in) {
        uint8_t *dst = (uint8_t *)data;
        for (uint16_t i = 0; i < len; i++) dst[i] = dbuf[i];
    }
    return ok;
}

// 前回の関数との互換用 (アドレス0に8バイトだけ要求)
bool ehci_get_device_descriptor8(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys, uint8_t out[8]
) {
    usb_setup_t s = mk_setup(0x80, 0x06, 0x0100, 0, 8);
    return ehci_control_transfer(op, head_qh, head_qh_phys, 0, 64, &s, out, true);
}

// ---- 列挙の結果 ----
typedef struct {
    uint8_t  addr;      // 割り当てたデバイスアドレス
    uint8_t  mps0;      // EP0の最大パケットサイズ
    uint8_t  config;    // bConfigurationValue
    uint8_t  iface;     // bInterfaceNumber

    uint8_t  ep_in;     // Bulk IN のエンドポイントアドレス
    uint8_t  ep_out;    // Bulk OUT のエンドポイントアドレス
    uint16_t mps_in;    // Bulk IN MPS
    uint16_t mps_out;   // Bulk OUT MPS

    // Bulk IN QH
    ehci_qh_t *qh_in;
    uint64_t qh_in_phys;

    // Bulk OUT QH
    ehci_qh_t *qh_out;
    uint64_t qh_out_phys;

} usb_msc_t;

// アドレス0にいるデバイスを列挙し、Mass Storage (Class 08 / SubClass 06 / Protocol 50) を探す
bool usb_enumerate_msc(
    volatile uint32_t *op,
    ehci_qh_t *head_qh,
    uint64_t head_qh_phys,
    usb_msc_t *dev
) {
    uint8_t buf[512];
    usb_setup_t s;

    // 1) High-speedデバイスのEP0最大パケットは仕様上64固定。
    //    8バイト読み(データ途中でSTATUSへ移る)は一部の実機がSTALLするため省略する
    const uint8_t mps0 = 64;

    // 2) SET_ADDRESS (アドレス1を割り当て)
    const uint8_t new_addr = 1;
    s = mk_setup(0x00, 0x05, new_addr, 0, 0);
    if (!ehci_control_transfer(op, head_qh, head_qh_phys, 0, mps0, &s, nullptr, false)) {
        put_text("enum: set address failed\n");
        return false;
    }
    wait_ms(10); // アドレス切替の回復時間 (仕様は2ms以上)

    // 3) Device記述子 全体(18バイト)
    s = mk_setup(0x80, 0x06, 0x0100, 0, 18);
    if (!ehci_control_transfer(op, head_qh, head_qh_phys, new_addr, mps0, &s, buf, true)) {
        put_text("enum: get device desc failed\n");
        return false;
    }
    put_text("VID=");  hex_put(buf[8]  | (buf[9]  << 8));
    put_text(" PID="); hex_put(buf[10] | (buf[11] << 8));
    put_text("\n");

    // 4) Configuration記述子: まず9バイトで全長を知る
    s = mk_setup(0x80, 0x06, 0x0200, 0, 9);
    if (!ehci_control_transfer(op, head_qh, head_qh_phys, new_addr, mps0, &s, buf, true)) {
        put_text("enum: get config9 failed\n");
        return false;
    }
    uint16_t total = buf[2] | (buf[3] << 8);
    if (total > sizeof(buf)) total = sizeof(buf);

    // 5) 全体を取得
    s = mk_setup(0x80, 0x06, 0x0200, 0, total);
    if (!ehci_control_transfer(op, head_qh, head_qh_phys, new_addr, mps0, &s, buf, true)) {
        put_text("enum: get config failed\n");
        return false;
    }

    // 6) 記述子を順にたどって Mass Storage / BOT のインターフェースを探す
    dev->addr = new_addr;
    dev->mps0 = mps0;
    dev->config = buf[5];
    dev->iface = 0;
    dev->ep_in = dev->ep_out = 0;
    dev->mps_in = dev->mps_out = 0;

    bool in_msc = false, found = false;
    uint32_t off = 0;
    while (off + 2 <= total) {
        uint8_t l = buf[off], t = buf[off + 1];
        if (l == 0) break;

        if (t == 4 && off + 9 <= total) {            // Interface
            in_msc = (buf[off + 3] == 0               // AlternateSetting 0
                   && buf[off + 5] == 0x08            // Mass Storage
                   && buf[off + 6] == 0x06            // SCSI transparent
                   && buf[off + 7] == 0x50);          // Bulk-Only Transport
            if (in_msc) {
                dev->iface = buf[off + 2];
                found = true;
            }
        } else if (t == 5 && in_msc && off + 7 <= total) { // Endpoint
            uint8_t  ep   = buf[off + 2];
            uint8_t  attr = buf[off + 3] & 0x03;
            uint16_t mps  = (buf[off + 4] | (buf[off + 5] << 8)) & 0x7FF;
            if (attr == 2) {                          // Bulk
                if (ep & 0x80) { dev->ep_in = ep;  dev->mps_in = mps; }
                else           { dev->ep_out = ep; dev->mps_out = mps; }
            }
        }
        off += l;
    }

    if (!found || dev->ep_in == 0 || dev->ep_out == 0) {
        put_text("enum: no mass storage (BOT) interface\n");
        return false;
    }

    // 7) SET_CONFIGURATION
    // 7) SET_CONFIGURATION
s = mk_setup(0x00, 0x09, dev->config, 0, 0);

if (!ehci_control_transfer(
        op,
        head_qh,
        head_qh_phys,
        new_addr,
        mps0,
        &s,
        nullptr,
        false)) {

    put_text("enum: set config failed\n");
    return false;
}


// 8) Bulk IN QHを作成

dev->qh_in_phys = alloc_page();

if (dev->qh_in_phys == 0) {
    put_text("enum: alloc bulk IN QH failed\n");
    return false;
}

dev->qh_in =
    (ehci_qh_t *)(directmap_addr_base + dev->qh_in_phys);

if (!ehci_init_qh(
        dev->qh_in,
        dev->qh_in_phys,
        dev->addr,
        dev->ep_in & 0x0F,
        2,              // High Speed
        dev->mps_in
    )) {

    put_text("enum: bulk IN QH failed\n");
    return false;
}


// 9) Bulk OUT QHを作成

dev->qh_out_phys = alloc_page();

if (dev->qh_out_phys == 0) {
    put_text("enum: alloc bulk OUT QH failed\n");
    return false;
}

dev->qh_out =
    (ehci_qh_t *)(directmap_addr_base + dev->qh_out_phys);

if (!ehci_init_qh(
        dev->qh_out,
        dev->qh_out_phys,
        dev->addr,
        dev->ep_out & 0x0F,
        2,              // High Speed
        dev->mps_out
    )) {

    put_text("enum: bulk OUT QH failed\n");
    return false;
}
return true;
}
// ============================================================
//  Mass Storage (Bulk-Only Transport) 読み出し層
// ============================================================

// ---- スクラッチバッファ (初回だけ確保) ----
static uint64_t bot_qtd_page  = 0; // qTD 用 (32B境界)
static uint64_t bot_data_page = 0; // データフェーズ用 (ページ境界, 最大4096B)
static uint64_t bot_sep_page  = 0; // CBW/CSW 用 (ページ境界)
static uint32_t bot_tag       = 0;

static void bot_ensure_buffers(void) {
    if (bot_qtd_page  == 0) bot_qtd_page  = alloc_page();
    if (bot_data_page == 0) bot_data_page = alloc_page();
    if (bot_sep_page  == 0) bot_sep_page  = alloc_page();
}

// Bulk転送を1回行う。
//   1つのqTDにlenバイトを載せるので、MPSを超える分はHCが自動で
//   マルチパケット化し、データトグルもHCが面倒を見てくれる。
//   toggle は「最初のパケット」のトグル。
static bool ehci_bulk_transfer(
    volatile uint32_t *op,
    ehci_qh_t *head_qh, uint64_t head_qh_phys,
    ehci_qh_t *bulk_qh, uint64_t bulk_qh_phys,
    uint64_t buf_phys, uint32_t len,
    bool dir_in, uint32_t toggle, uint32_t *actual)
{
    if (bulk_qh == nullptr) return false;
    if (len == 0 || len > 4096) return false;   // 1ページまで

    bot_ensure_buffers();
    if (bot_qtd_page == 0) return false;

    volatile ehci_qtd_t *qtd =
        (volatile ehci_qtd_t *)(directmap_addr_base + bot_qtd_page);
    uint32_t qtd_phys = (uint32_t)bot_qtd_page;

    // ゼロ初期化
    volatile uint8_t *z = (volatile uint8_t *)qtd;
    for (uint32_t i = 0; i < sizeof(ehci_qtd_t); i++) z[i] = 0;

    qtd->next_qtd     = 1; // T: チェーン終端
    qtd->alt_next_qtd = 1; // T

    // バッファポインタ (buffer[0]の下位12bitがオフセット)
    uint64_t base = buf_phys & ~0xFFFULL;
    uint32_t off  = (uint32_t)(buf_phys & 0xFFF);
    for (uint32_t i = 0; i < 5; i++)
        qtd->buffer[i] = (uint32_t)(base + (uint64_t)i * 4096);
    qtd->buffer[0] = (uint32_t)(base + off);

    qtd->token = qtd_token(dir_in ? QTD_PID_IN : QTD_PID_OUT, len, toggle);

    // QH をこのqTDへ差し向ける
    bulk_qh->next_qtd     = qtd_phys;
    bulk_qh->alt_next_qtd = 1;
    bulk_qh->token        = 0;

    __sync_synchronize();

    // 非同期リスト先頭に挿入
    bulk_qh->horizontal_link = head_qh->horizontal_link;
    __sync_synchronize();
    head_qh->horizontal_link = (uint32_t)bulk_qh_phys | 0x02;
    __sync_synchronize();

    // 完了待ち
    bool ok = false;
    uint64_t start = timer();
    while (1) {
        uint32_t t = qtd->token;

        if (t & QTD_ERRMASK) {
            put_text("bulk error T="); hex_put(t); put_text("\n");
            break;
        }
        if ((t & QTD_ACTIVE) == 0) { ok = true; break; }

        if ((timer() - start) * (hpetcount / 1000000) >= 2000000000ULL) {
            put_text("bulk timeout T="); hex_put(t); put_text("\n");
            break;
        }
        __asm__ volatile("pause");
    }

    if (actual) {
        uint32_t remain = (qtd->token >> 16) & 0x7FFF;
        *actual = (remain > len) ? 0 : (len - remain);
    }

    // リストから外してドアベル
    head_qh->horizontal_link = (uint32_t)head_qh_phys | 0x02;
    __sync_synchronize();
    op[EHCI_USBCMD] |= USBCMD_IAAD;
    wait_reg(&op[EHCI_USBSTS], USBSTS_IAA, USBSTS_IAA, 100000000ULL);
    op[EHCI_USBSTS] = USBSTS_IAA;

    return ok;
}

// Bulk-Only Mass Storage Reset (class request 0xFF, interface)
static bool msc_bulk_only_reset(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev)
{
    usb_setup_t s = mk_setup(0x21, 0xFF, 0, dev->iface, 0);
    return ehci_control_transfer(op, head_qh, head_qh_phys,
                                 dev->addr, dev->mps0, &s, nullptr, false);
}

// Clear Feature(ENDPOINT_HALT): データトグルを DATA0 に戻す
static bool msc_clear_halt(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev, uint8_t ep)
{
    usb_setup_t s = mk_setup(0x02, 0x01, 0, ep, 0);
    return ehci_control_transfer(op, head_qh, head_qh_phys,
                                 dev->addr, dev->mps0, &s, nullptr, false);
}

// BOT の1コマンド: CBW → (データ) → CSW
//   data     : データフェーズのバッファ (INなら受信先 / OUTなら送信元)
//   data_len : データ長 (0 ならデータフェーズなし, 最大4096)
//   dir_in   : データフェーズの向き
static bool msc_bot_command(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev,
    const uint8_t *cdb, uint8_t cdb_len,
    void *data, uint32_t data_len, bool dir_in)
{
    if (data_len > 4096) return false;

    bot_ensure_buffers();
    if (bot_sep_page == 0) return false;

    volatile uint8_t *sep  = (volatile uint8_t *)(directmap_addr_base + bot_sep_page);
    volatile uint8_t *dbuf = (volatile uint8_t *)(directmap_addr_base + bot_data_page);

    // 毎回トグルを DATA0 に戻す (前回の状態に依存しない)
    msc_clear_halt(op, head_qh, head_qh_phys, dev, dev->ep_in);
    msc_clear_halt(op, head_qh, head_qh_phys, dev, dev->ep_out);
    msc_bulk_only_reset(op, head_qh, head_qh_phys, dev);

    // ---- CBW (Bulk OUT, 31バイト, トグル DATA0) ----
    usb_msc_cbw_t *cbw = (usb_msc_cbw_t *)sep;
    uint8_t *cb = (uint8_t *)cbw;
    for (uint32_t i = 0; i < sizeof(usb_msc_cbw_t); i++) cb[i] = 0;

    cbw->signature            = 0x43425355;      // "USBC"
    cbw->tag                  = ++bot_tag;
    cbw->data_transfer_length = data_len;
    cbw->flags                = dir_in ? 0x80 : 0x00;
    cbw->lun                  = 0;
    cbw->cb_length            = cdb_len;
        for (uint8_t i = 0; i < cdb_len && i < 16; i++) cbw->cb[i] = cdb[i];

    uint32_t actual = 0;
    if (!ehci_bulk_transfer(op, head_qh, head_qh_phys,
                            dev->qh_out, dev->qh_out_phys,
                            bot_sep_page, 31, false, 0, &actual)) {
        put_text("msc: CBW failed\n");
        return false;
    }

    // ---- データフェーズ ----
    if (data_len > 0) {
        if (!dir_in) {
            const uint8_t *src = (const uint8_t *)data;
            for (uint32_t i = 0; i < data_len; i++) dbuf[i] = src[i];
        }
        if (!ehci_bulk_transfer(op, head_qh, head_qh_phys,
                                dev->qh_in, dev->qh_in_phys,
                                bot_data_page, data_len, dir_in, 0, &actual)) {
            put_text("msc: data phase failed\n");
            // それでも CSW を読みにいく
        }
    }

    // ---- CSW (Bulk IN, 13バイト) ----
    // INデータフェーズで送られたパケット数の偶奇がCSWのトグルになる
    uint32_t mps = dev->mps_in ? dev->mps_in : 512;
    uint32_t pkts = data_len ? (data_len + mps - 1) / mps : 0;
    uint32_t csw_toggle = dir_in ? (pkts & 1) : 0;

    if (!ehci_bulk_transfer(op, head_qh, head_qh_phys,
                            dev->qh_in, dev->qh_in_phys,
                            bot_sep_page + 64, 13, true, csw_toggle, &actual)) {
        put_text("msc: CSW failed\n");
        return false;
    }

    volatile uint8_t *cswp = sep + 64;
    uint32_t sig = (uint32_t)cswp[0] | ((uint32_t)cswp[1] << 8)
                 | ((uint32_t)cswp[2] << 16) | ((uint32_t)cswp[3] << 24);
    if (sig != 0x53425355) {              // "USBS"
        put_text("msc: bad CSW signature\n");
        return false;
    }
    uint8_t status = cswp[12];
    if (status != 0x00) {
        put_text("msc: CSW status="); hex_put(status); put_text("\n");
        return false;
    }

    // INデータを呼び出し元へコピー
    if (data_len > 0 && dir_in && data) {
        uint8_t *outb = (uint8_t *)data;
        for (uint32_t i = 0; i < data_len; i++) outb[i] = dbuf[i];
    }
    return true;
}
// ---- SCSI コマンド ----

// INQUIRY (任意): 36バイトの標準INQUIRYデータ
bool usb_msc_inquiry(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev, uint8_t out[36])
{
    uint8_t cdb[6] = {0};
    cdb[0] = 0x12;          // INQUIRY
    cdb[4] = 36;            // Allocation Length
    return msc_bot_command(op, head_qh, head_qh_phys, dev,
                           cdb, 6, out, 36, true);
}

// READ CAPACITY(10): 最終LBAとブロック長
bool usb_msc_read_capacity(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev, uint32_t *last_lba, uint32_t *block_size)
{
    uint8_t cdb[10] = {0};
    cdb[0] = 0x25;          // READ CAPACITY(10)
    uint8_t resp[8];
    if (!msc_bot_command(op, head_qh, head_qh_phys, dev, cdb, 10, resp, 8, true))
        return false;

    if (last_lba) {
        *last_lba = ((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16)
                  | ((uint32_t)resp[2] << 8)  |  (uint32_t)resp[3];
    }
    if (block_size) {
        *block_size = ((uint32_t)resp[4] << 24) | ((uint32_t)resp[5] << 16)
                    | ((uint32_t)resp[6] << 8)  |  (uint32_t)resp[7];
    }
    return true;
}

// READ(10): LBAからcountセクタ (1回あたり最大8セクタ = 4096B)
bool usb_msc_read10(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev, uint32_t lba, uint16_t count, void *out)
{
    if (count == 0 || count > 8) return false;

    uint8_t cdb[10] = {0};
    cdb[0] = 0x28;          // READ(10)
    cdb[2] = (lba >> 24) & 0xFF;
    cdb[3] = (lba >> 16) & 0xFF;
    cdb[4] = (lba >> 8)  & 0xFF;
    cdb[5] =  lba        & 0xFF;
    cdb[7] = (count >> 8) & 0xFF;
    cdb[8] =  count       & 0xFF;

    return msc_bot_command(op, head_qh, head_qh_phys, dev,
                           cdb, 10, out, (uint32_t)count * 512, true);
}

// 任意セクタ数を読む (内部で8セクタずつに分割)
bool usb_msc_read(
    volatile uint32_t *op, ehci_qh_t *head_qh, uint64_t head_qh_phys,
    usb_msc_t *dev, uint32_t lba, uint32_t count, void *out)
{
    uint8_t *dst = (uint8_t *)out;
    while (count > 0) {
        uint16_t n = (count > 8) ? 8 : (uint16_t)count;
        if (!usb_msc_read10(op, head_qh, head_qh_phys, dev, lba, n, dst))
            return false;
        dst   += (uint32_t)n * 512;
        lba   += n;
        count -= n;
    }
    return true;
}
// 呼び出し例 (usb_init の ehci_init_ports の後ろ):
//
//   usb_msc_t msc;
//   if (usb_enumerate_msc(op, qh, qh_phys, &msc)) {
//       // 次: BOTでSCSI (INQUIRY / READ CAPACITY / READ(10))
//   }
#define QTD_HALTED (1u << 6)

void usb_init(uint64_t ehci_mmio_virtual){
    volatile uint8_t *ehci_mmio = (uint8_t*)ehci_mmio_virtual;
    uint8_t caplength =
        *(volatile uint8_t *)(ehci_mmio + 0x00);
    uint16_t hciversion =
        *(volatile uint16_t *)(ehci_mmio + 0x02);
    uint32_t hcsparams =
        *(volatile uint32_t *)(ehci_mmio + 0x04);
    uint32_t hccparams =
        *(volatile uint32_t *)(ehci_mmio + 0x08);
    uint32_t eecp = (hccparams >> 8) & 0xFF;
    volatile uint32_t *op =
        (volatile uint32_t *)(ehci_mmio + caplength);
    (void)hciversion;

    uint32_t port_count = hcsparams & 0x0F;

    uint16_t cmd = *(uint16_t*)pci_ecam_addr(0, ehcidevice, ehcifunction, 0x04);
    cmd |= (1u << 1);  // Memory Space Enable
    cmd |= (1u << 2);  // Bus Master Enable
    *(uint16_t*)pci_ecam_addr(0, ehcidevice, ehcifunction, 0x04) = cmd;
    put_text("ai");

    // BIOSからEHCIの所有権を取得 (EECPから探す)
    ehci_bios_handoff(eecp);

    uint32_t usbcmd = op[0];
    uint32_t usbsts = op[1];
    put_text("//");
    hex_put(usbcmd);
    put_text(",");
    hex_put(usbsts);
    put_text("//");

    if(!ehci_stop_and_reset(op)) kernelpanic();

    put_text("os");
    op[EHCI_USBINTR] = 0;

    uint64_t qh_phys = alloc_page();

    ehci_qh_t *qh =
        (ehci_qh_t *)(directmap_addr_base + qh_phys);
    ehci_init_qh(qh, qh_phys);
    put_text("aaa\n");
    if (!ehci_start_async(op, qh_phys)) {
        kernelpanic();
    }
    put_text("bbb");
    hex_put(port_count);

    // EHCIにポートの所有権を割り当てる (RS=1にした後でCONFIGFLAGをセット)
    op[EHCI_CONFIGFLAG] = 1;
    wait_ms(5);

    // ポートに電源を入れて接続を安定させる
    ehci_power_ports(op, hcsparams, port_count);

    put_text("ass\n");
    clearscreen();

    put_text("before reset\n");
    ehci_dump_ports(op, port_count);

    // 接続されているデバイスのポートをリセット・有効化
    ehci_reset_port(op,0);
    put_text("\n");
    ehci_dump_ports(op, port_count);

    wait_ns(3000000000ULL);
   clearscreen();
   ehci_dump_ports(op, port_count);
       uint8_t d[8];
    if (ehci_get_device_descriptor8(op, qh, qh_phys, d)) {
        for (int i = 0; i < 8; i++) { hex_put(d[i]); put_text(" "); }
        put_text("\n");
    }

    usb_msc_t msc;
    if (!usb_enumerate_msc(op, qh, qh_phys, &msc)) {
        put_text("no usb memory\n");
        return;
    }

    // 容量を表示
    uint32_t last_lba = 0, blk = 0;
    if (usb_msc_read_capacity(op, qh, qh_phys, &msc, &last_lba, &blk)) {
        put_text("USB memory: last LBA="); hex_put(last_lba);
        put_text(" block=");               hex_put(blk);
        put_text("\n");
    }

    // セクタ0を読んで先頭16バイトを表示 (MBR/ブートセクタ等)
    static uint8_t secbuf[512];
    if (usb_msc_read(op, qh, qh_phys, &msc, 0, 1, secbuf)) {
        put_text("sector 0: ");
        for (int i = 0; i < 16; i++) { hex_put(secbuf[i]); put_text(" "); }
        put_text("\n");
    } else {
        put_text("read sector 0 failed\n");
    }
}

uint64_t timer(){
    return (*(uint64_t*)(directmap_addr_base + hpetaddr + 0xF0));
}
void main(){
  init_maxpaging();
  

  next_row();
  init_ram();
  init_acpi();
  next_row();
  direct_map(0xc0000000);

  
  asm volatile ("int $3");
  
 
  hex_put(*(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x20));
  put_text("kkk");
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x320) = 1 << 16;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x330) = 1 << 16;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x340) = 1 << 16;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x350) = 1 << 16;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x360) = 1 << 16;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x370) = 1 << 16;
  uint32_t svr = *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0xF0);
  svr |= (1 << 8);
  svr &= ~0xFF;
  svr |= 0xFF;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0xF0) = svr;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x80) = 0;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x3E0) = 3;
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x320) = 0x20 | (1 << 17);
  *(uint32_t*)(directmap_addr_base+ 0xFEE00000 + 0x380) = 50000;
  
  outb(0x21, 0xFF);  // PIC1 IRQ0～7を全マスク
  outb(0xA1, 0xFF);  // PIC2 IRQ8～15を全マスク
  put_text("\n");
  
  pci_scan();
  uint64_t kari3 = *(uint64_t*)(directmap_addr_base + hpetaddr);
  hpetcount = (kari3 >> 32);
  *(uint64_t*)(directmap_addr_base + hpetaddr + 0x10) |= 1ull;
 
  put_text("\n");
  ehciaddr = (uint64_t)(ehciaddr & ~0x0Fu);
  usb_init(directmap_addr_base + ehciaddr);
  while(1);
  
  return;
}