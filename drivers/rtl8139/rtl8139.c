#include "drivers/rtl8139/rtl8139.h"
#include "drivers/pci/pci.h"
#include "drivers/ports/ports.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "net/ethernet.h"

#define RTL_VENDOR_ID             0x10EC
#define RTL_DEVICE_ID             0x8139

#define RTL_RX_BUFFER_SIZE        9728
#define RTL_TX_BUFFER_COUNT       4
#define RTL_TX_BUFFER_SIZE        2048

#define RTL_REG_MAC0              0x00
#define RTL_REG_TX_STATUS0        0x10
#define RTL_REG_TX_ADDR0          0x20
#define RTL_REG_RX_BUF            0x30
#define RTL_REG_CAPR              0x38
#define RTL_REG_IMR               0x3C
#define RTL_REG_ISR               0x3E
#define RTL_REG_TCR               0x40
#define RTL_REG_RCR               0x44
#define RTL_REG_CONFIG1           0x52
#define RTL_REG_COMMAND           0x37

#define RTL_CMD_RX_ENABLE         0x08
#define RTL_CMD_TX_ENABLE         0x04
#define RTL_CMD_RESET             0x10
#define RTL_CMD_RX_BUFFER_EMPTY   0x01

#define RTL_ISR_RX_OK             0x0001
#define RTL_ISR_RX_ERR            0x0002
#define RTL_ISR_TX_OK             0x0004
#define RTL_ISR_TX_ERR            0x0008
#define RTL_ISR_RX_OVERFLOW       0x0010

#define RTL_RCR_ACCEPT_ALL_PHYS   0x00000001
#define RTL_RCR_ACCEPT_MY_PHYS    0x00000002
#define RTL_RCR_ACCEPT_MULTICAST  0x00000004
#define RTL_RCR_ACCEPT_BROADCAST  0x00000008
#define RTL_RCR_WRAP              0x00000080

#define RTL_RX_RING_SIZE          8192 

#define RTL_RCR_RBLEN_8K          0x00000000 // Биты 11-12 равны 00

#define RTL_RX_STATUS_OK          0x0001

static uint32_t io_base = 0;
static int rtl_ready = 0;
static uint32_t rx_cur = 0;
static uint8_t tx_slot = 0;

volatile int net_packet_received = 0;
uint8_t rtl_mac[6];

static uint8_t rx_buffer[RTL_RX_BUFFER_SIZE + 16 + 1500] __attribute__((aligned(4096)));
static uint8_t tx_buffers[RTL_TX_BUFFER_COUNT][RTL_TX_BUFFER_SIZE] __attribute__((aligned(16)));

void set_net_packet_flag() {
    net_packet_received = 1;
}

int rtl8139_is_ready() {
    return rtl_ready;
}

static void rtl8139_acknowledge(uint16_t status) {
    if (!rtl_ready) {
        return;
    }
    outw(io_base + RTL_REG_ISR, status);
}

void rtl8139_poll() {
    if (!rtl_ready) {
        return;
    }

    uint16_t status = inw(io_base + RTL_REG_ISR);

    if (status & (RTL_ISR_TX_OK | RTL_ISR_TX_ERR)) {
        rtl8139_acknowledge(status & (RTL_ISR_TX_OK | RTL_ISR_TX_ERR));
    }

    // Взводим флаг, только если чип аппаратно подтверждает: пакет в ОЗУ!
    if (status & RTL_ISR_RX_OK) {
        net_packet_received = 1;
    }
}

void rtl8139_handle_irq() {
    uint16_t status = inw(io_base + RTL_REG_ISR);

    if (!rtl_ready) {
        return;
    }

    // Вызываем обработку, только если чип сам сказал: "Я успешно записал пакет в ОЗУ!"
    if (status & RTL_ISR_RX_OK) {
        rtl8139_handle_receive();
    }

    // Очищаем обработанные прерывания (записью единицы в бит)
    if (status & (RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW | RTL_ISR_TX_OK | RTL_ISR_TX_ERR)) {
        outw(io_base + RTL_REG_ISR, status);
    }
}
void rtl8139_init() {
    pci_device_t dev = pci_find_device(RTL_VENDOR_ID, RTL_DEVICE_ID);

    if (dev.vendor_id == 0xFFFF) {
      terminal_write("RTL8139 not found.\n");
        return;
    }

    pci_enable_bus_mastering(dev.bus, dev.slot, dev.func);
    io_base = pci_get_bar(dev.bus, dev.slot, dev.func, 0) & ~0x3;

    outb(io_base + RTL_REG_CONFIG1, 0x00);
    outb(io_base + RTL_REG_COMMAND, RTL_CMD_RESET);
    while (inb(io_base + RTL_REG_COMMAND) & RTL_CMD_RESET) {
    }

    for (int i = 0; i < 6; i++) {
        rtl_mac[i] = inb(io_base + RTL_REG_MAC0 + i);
    }

rx_cur = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));
    memset(tx_buffers, 0, sizeof(tx_buffers));

    outl(io_base + RTL_REG_RX_BUF, (uint32_t)rx_buffer);
    outl(io_base + RTL_REG_TCR, 0x03000600);
    outl(io_base + RTL_REG_RCR,
         RTL_RCR_ACCEPT_ALL_PHYS |
         RTL_RCR_ACCEPT_MY_PHYS |
         RTL_RCR_ACCEPT_MULTICAST |
         RTL_RCR_ACCEPT_BROADCAST |
         RTL_RCR_RBLEN_8K |
         RTL_RCR_WRAP); 
         
    // Записываем 0xFFF0, НО сразу после этого принудительно очищаем ISR
    outw(io_base + RTL_REG_CAPR, 0xFFF0); 
    outw(io_base + RTL_REG_IMR, RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_TX_OK | RTL_ISR_TX_ERR);
    
    // Включаем RX/TX только ПОСЛЕ того, как все указатели установлены
    outb(io_base + RTL_REG_COMMAND, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);

    rtl_ready = 1;
    rtl8139_acknowledge(0xFFFF);

    terminal_write("MAC: ");
    for (int i = 0; i < 6; i++) {
        terminal_write_hex(rtl_mac[i]);
        if (i < 5) {
            terminal_write(":");
        }
    }
    terminal_write(" IRQ ");
    terminal_write_dec(pci_get_irq_line(dev.bus, dev.slot, dev.func));
    terminal_write("\n");
}

void rtl8139_send_packet(void* data, uint32_t length) {
    uint8_t slot;

    if (!rtl_ready || data == 0 || length == 0 || length > RTL_TX_BUFFER_SIZE) {
        return;
    }

    slot = tx_slot;
    tx_slot = (tx_slot + 1) % RTL_TX_BUFFER_COUNT;

    memcpy(tx_buffers[slot], data, length);
    outl(io_base + RTL_REG_TX_ADDR0 + slot * 4, (uint32_t)tx_buffers[slot]);
    outl(io_base + RTL_REG_TX_STATUS0 + slot * 4, length & 0x1FFF);
}

#define RTL_REG_CBR 0x3A // Регистр текущего адреса записи чипа (Current Buffer Address)

void rtl8139_handle_receive() {
    if (!rtl_ready) {
        return;
    }

    int max_packets = 16; 

    while (!(inb(io_base + RTL_REG_COMMAND) & RTL_CMD_RX_BUFFER_EMPTY)) {
        if (max_packets-- <= 0) {
            break;
        }

        uint32_t offset = rx_cur % 8192;
        
        __asm__ volatile("" ::: "memory");

        volatile uint16_t* status_ptr = (volatile uint16_t*)(rx_buffer + offset);
        volatile uint16_t* len_ptr = (volatile uint16_t*)(rx_buffer + offset + 2);
        
        // Короткое ожидание DMA
        int timeout = 100;
        while (*status_ptr == 0 && *len_ptr == 0 && timeout > 0) {
            __asm__ volatile("pause");
            timeout--;
        }

        uint16_t status = *status_ptr;
        uint16_t packet_len = *len_ptr;
        uint8_t* packet = rx_buffer + offset + 4;

        // --- ФИКС ХАНГ-АУТА (НУЛЕВОЙ ПУЛЕМЕТ) ---
        if (status == 0 && packet_len == 0) {
            // Читаем, куда чип СЕЙЧАС аппаратно указывает в буфере
            uint16_t cbr = inw(io_base + RTL_REG_CBR);
            
            // Принудительно выравниваем и переносим наш указатель туда, где находится чип
            rx_cur = (cbr + 3) & ~3;
            
            // Сообщаем чипу, что мы «прочитали» всё вплоть до этой точки
            uint32_t capr_value = (rx_cur - 0x10 + 8192) % 8192;
            outw(io_base + RTL_REG_CAPR, (uint16_t)capr_value);
            
            terminal_write("[RTL8139] DMA Sync Reset applied via CBR\n");
            break; // Выходим из прерывания, пулемет остановлен
        }

        // Проверка флага успешного приема и ограничений по размеру
        if (!(status & RTL_RX_STATUS_OK) || packet_len < 4 || packet_len > 1518 + 4) {
            terminal_write("[RTL8139] Bad Frame Header. Resetting ring.\n");
            
            // Если заголовок просто побит мусором, сбрасываем указатель на CBR чипа
            uint16_t cbr = inw(io_base + RTL_REG_CBR);
            rx_cur = (cbr + 3) & ~3;
            uint32_t capr_value = (rx_cur - 0x10 + 8192) % 8192;
            outw(io_base + RTL_REG_CAPR, (uint16_t)capr_value);
            
            rtl8139_acknowledge(RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW);
            break;
        }

        // Если пакет валидный — обрабатываем!
        ethernet_receive_frame(packet, packet_len - 4);

        // Продвигаем указатели для успешного пакета
        rx_cur = (rx_cur + packet_len + 4 + 3) & ~3;
        uint32_t capr_value = (rx_cur - 0x10 + 8192) % 8192;
        outw(io_base + RTL_REG_CAPR, (uint16_t)capr_value);
        rx_cur %= 8192;
        
        __asm__ volatile("" ::: "memory");
    }

    // Сброс флагов прерываний
    rtl8139_acknowledge(RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW);
    net_packet_received = 0;
}