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
#define USBCMD_ASE         (1u << 5)

#define USBSTS_HCHALTED    (1u << 12)
#define USBSTS_ASS         (1u << 15)
#define PORTSC_CCS   (1u << 0)  // Current Connect Status
#define PORTSC_CSC   (1u << 1)  // Connect Status Change
#define PORTSC_PED   (1u << 2)  // Port Enable
#define PORTSC_PE    (1u << 2)
#define PORTSC_PEC   (1u << 3)
#define PORTSC_OCC (1u << 5)
#define PORTSC_PR    (1u << 8)  // Port Reset
#define PORTSC_PP    (1u << 12) // Port Power
#define EHCI_LEGSUP_OS_OWNED   (1u << 24)
#define EHCI_LEGSUP_BIOS_OWNED (1u << 16)
bool wait_reg(
    volatile uint32_t *reg,
    uint32_t mask,
    uint32_t expected,
    uint64_t timeout_ns
)
{
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

    // qTDなし（Tビットをセット）
    qh->next_qtd = 1;
    qh->alt_next_qtd = 1;

    return true;
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

    uint64_t start = timer();

    while (1) {
        uint32_t sts = op[EHCI_USBSTS];

        if ((sts & USBSTS_HCHALTED) == 0) {
            put_text("HCHalted cleared\n");
        }

        if (sts & USBSTS_ASS) {
            put_text("Async enabled\n");
            return true;
        }

        uint64_t elapsed = timer() - start;
        elapsed *= hpetcount / 1000000;

        if (elapsed >= 100000000ULL) {
            put_text("Async timeout CMD=");
            hex_put(op[EHCI_USBCMD]);
            put_text(" STS=");
            hex_put(op[EHCI_USBSTS]);
            put_text("\n");
            return false;
        }

        __asm__ volatile("pause");
    }
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
void wait_ns(uint64_t time1){
  uint64_t timerstart = timer();

    while (1) {

        uint64_t time = timer() - timerstart;
        time *= hpetcount / 1000000;

        if (time >= time1) {
            return;
        }

        __asm__ volatile("pause");
    }
}
bool ehci_reset_port(
    volatile uint32_t *op,
    uint32_t port_index
) {
    uint32_t portsc = ehci_port_read(op, port_index);

    // 接続されていなければ失敗
    if ((portsc & PORTSC_CCS) == 0) {
        return false;
    }

    // 変更通知ビットを除外し、既存の状態を維持する
    uint32_t value = portsc &
    ~(PORTSC_PE | PORTSC_PR |
      PORTSC_CSC | PORTSC_PEC | PORTSC_OCC);

value |= PORTSC_PR;
ehci_port_write(op, port_index, value);

    // USB仕様に従い、リセットを少なくとも50ms維持する
    // この関数はOSのタイマーに置き換えることを推奨
    uint64_t timerstart = timer();
    while(1){
      uint64_t time = timer();
      time -= timerstart;
      time *= hpetcount / 1000000;
      
      if(time > 5000000){
        break;
      }
      __asm__ volatile("pause");
    }


    // 現在値を再取得し、リセット解除
    portsc = ehci_port_read(op, port_index);

value = portsc &
    ~(PORTSC_PR | PORTSC_CSC |
      PORTSC_PEC | PORTSC_OCC);

ehci_port_write(op, port_index, value);

    // ポートが有効になるまで待つ
    for (uint32_t i = 0; i < 1000000; i++) {
        portsc = ehci_port_read(op, port_index);

        if ((portsc & PORTSC_CCS) == 0) {
            return false;
        }

        if ((portsc & PORTSC_PE) != 0 &&
            (portsc & PORTSC_PR) == 0) {
            return true;
        }
    }

    return false;
}
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
uint16_t cmd = *(uint16_t*)pci_ecam_addr(0, ehcidevice, ehcifunction, 0x04);
cmd |= (1u << 1);  // Memory Space Enable
cmd |= (1u << 2);  // Bus Master Enable
*(uint16_t*)pci_ecam_addr(0, ehcidevice, ehcifunction, 0x04) = cmd;
put_text("ai");
volatile uint8_t *os_owned =
    (volatile uint8_t *)pci_ecam_addr(
        0, ehcidevice, ehcifunction, 0x6B
    );

*os_owned |= 1;
uint32_t usbcmd = op[0];
uint32_t usbsts = op[1];
put_text("//");
hex_put(usbcmd);
put_text(",");
hex_put(usbsts);
put_text("//");


if(!ehci_stop_and_reset(op)) kernelpanic();

put_text("os");
op[2] = 0;  // USBINTR = 0
// EHCIにポートの所有権を割り当てる
*(volatile uint32_t *)((volatile uint8_t *)op + 0x40) = 1;
uint64_t qh_phys = alloc_page();

ehci_qh_t *qh =
    (ehci_qh_t *)(directmap_addr_base + qh_phys);
ehci_init_qh(qh, qh_phys);
put_text("aaa\n");
if (!ehci_start_async(op, qh_phys)) {
    kernelpanic();
}
put_text("bbb");
hex_put(hcsparams & 0x0F);
put_text("ass");
clearscreen();
ehci_dump_ports(op,hcsparams & 0x0f);
wait_ns(3000000000);
clearscreen();
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