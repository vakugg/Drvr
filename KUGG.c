/*  
 *  KUGG.c -  The network driver tutorial for Linux of XX version.
 *  Copyright(c) im github-shifted nonommercial example of something, don't encourage me plz.
 */
#include <linux/module.h> // грузим из ядра/в ядро	
#include <linux/init.h>	// specify initialization and cleanup functions ю ноу...
#include <linux/pci.h>  // PCI либа 
#include <linux/netdevice.h> // net_device structure defined here
#include <linux/etherdevice.h> // alloc_etherdev - аллоцируем девайс из структуры а если мы хотим конкретно указать это оптоволокно или что-то ещё, то нам нужна другая либа
#include <linux/interrupt.h> // request_irq - просим интеррапшены


#define KUGG_DRV_VERSION "1.0.0.1"
char KUGG_driver_name[] = "KUGG";
char KUGG_driver_version[] = KUGG_DRV_VERSION;

/* про железнявые параметры (с.м. на потолке) */
#define VENDER_ID			0x1969
#define DEVICE_ID			0x1062
#define REG_MAC_STA_ADDR	0x1490 // смещение для регистра с MAC адресом 
#define NUM_TX_DESC 		4 // from 4 to 1024 for different hardware
#define REG_MASTER_CTRL		0x1400 // soft reset register offset
#define SOFT_RESET_CMD		0x41	// BIT(0) | BIT(6)
/* про железнявые параметры - всё */

/*sizes*/
#define MTU_SIZE			1500
#define HDR_SIZE			14
#define CS_SIZE			4

#define TX_BUF_SIZE  		(MTU_SIZE + HDR_SIZE + CS_SIZE) /* 1536 should be at least MTU + 14 + 4 */


#define TOTAL_TX_BUF_SIZE	(TX_BUF_SIZE * NUM_TX_DESC)
#define TOTAL_RX_BUF_SIZE	16000  // тут должен быть желаемый размер и не один бы, а чтобы можно было конфигурить, но как есть.
/*sizes*/


// Используй документацию: документацию с.м. на потолке
static DEFINE_PCI_DEVICE_TABLE(KUGG_pci_tbl) = 
{
	{PCI_DEVICE(VENDER_ID, DEVICE_ID)},
	/* required last entry */
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, KUGG_pci_tbl);

MODULE_AUTHOR("KUDD DAVL  <kugg.davl@needjob.ru>");
MODULE_DESCRIPTION("   - for Linux version XX");
MODULE_DESCRIPTION("sample of <Linux Ethernet device driver demystified>");
MODULE_LICENSE("GPL");
MODULE_VERSION(KUGG_DRV_VERSION);

/*ну, это буфер*/
struct KUGG_buffer 
{
	struct sk_buff *skb;	/* socket buffer */
	u16 length;		/* rx buffer length */
	u16 flags;		/* information of buffer */
	dma_addr_t dma;
};

/* board specific private data structure */
struct KUGG_adapter 
{
	struct net_device	*netdev;
	struct pci_dev	*pdev;
	spinlock_t	    lock;
	bool 			have_msi;
	u8 __iomem		*hw_addr;  /* виртуальнй адреc MMIO */
	unsigned long	regs_len;   /* Длина регитров */
	unsigned int	cur_tx;
	unsigned int	dirty_tx;
	unsigned char	*tx_buf[NUM_TX_DESC];
	unsigned char 	*tx_bufs;        /* Tx buffer start address (virtual address). */
	dma_addr_t 		tx_bufs_dma;      /* Tx buffer dma address (physical address) */

	struct net_device_stats stats;
	unsigned char *rx_ring;
	dma_addr_t rx_ring_dma;
	unsigned int cur_rx;
};

/* интеррапшены получения|отправки */
static irqreturn_t KUGG_interrupt (int irq, void *dev_instance) 
{
	struct net_device *netdev = (struct net_device*)dev_instance;
	struct KUGG_adapter *adapter = netdev_priv(netdev);

	// мы хотим различать это чей интеррапшн, но не хотим это кодить 

	printk(KERN_INFO "Now calls KUGG_interrupt(). irq is %d.\n", irq);

	return IRQ_HANDLED;
}

/* Инитим кольцевой буфер отправки*/
static void KUGG_init_ring (struct net_device *netdev)
{
	struct KUGG_adapter *adapter = netdev_priv(netdev);
	int i;

	adapter->cur_tx = 0;
	adapter->dirty_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++)
		adapter->tx_buf[i] = &adapter->tx_bufs[i * TX_BUF_SIZE];
        
	return;
}

/*мне попалась реализация с мягким ресетом - пусть будет*/
static void KUGG_chip_reset (void *ioaddr)
{
    	u32 ctrl_data = 0; // control data write to register

    	// read control register value
    	*(u32 *)&ctrl_data = readl(ioaddr + REG_MASTER_CTRL);
    	// add soft reset control bit
    	ctrl_data |= SOFT_RESET_CMD;

        /* Soft reset the chip. */
        writel(ctrl_data, ioaddr + REG_MASTER_CTRL);
        // wait 20ms for reset ready
        msleep(20);

    	printk(KERN_INFO "reset chip ready .\n");

        return;
}

// set tx DMA address. start xmit.
static void KUGG_hw_start (struct net_device *netdev)
{
	struct KUGG_adapter *adapter = netdev_priv(netdev);
	void *ioaddr = adapter->hw_addr;

	// reset the chip
	KUGG_chip_reset(ioaddr);

	/* Здесь должны быть методы:*/

	/* Выкл/Вкылы буферов отправки/полуения*/

	/* Конфиги буферов: скорости, задержки приёма/отправки пакетов, и то угодно ещё что нам нужно*/

	/* Иниты буферов на DMA*/

	/* Инит битмаки интррапшенов (или не битмаски)*/

	netif_start_queue (netdev);
	return;
}


//"обработчик" входного буфера, выделение памти под него(буфер кольцевой)
static int KUGG_open(struct net_device *netdev)
{
	int retval;
	struct KUGG_adapter *adapter = netdev_priv(netdev); //возвращает указатель на используемую драйвером область структуры сетевого устройства
	printk(KERN_INFO "Now calls KUGG_open().\n");

	// enable MSI, can get IRQ 43 - Интеррапшен через сообщение = можно
	adapter->have_msi = true;
	retval = pci_enable_msi(adapter->pdev);
	printk(KERN_INFO "retval is %d, adapter->pdev->irq is %d, .\n", retval,  adapter->pdev->irq);
	/* читаем примеры и видим:
	 * get the IRQ,  43 is correct, 17 is wrong
	 * second arg is interrupt handler
	 * third is flags, 0 means no IRQ sharing
	 */
	retval = request_irq(adapter->pdev->irq, KUGG_interrupt, 0, netdev->name, netdev);
	if(retval)
	{
		printk(KERN_INFO "request_irq() failed. netdev->irq is %d, adapter->pdev->irq is %d, .\n", netdev->irq, adapter->pdev->irq);
		free_irq(adapter->pdev->irq, netdev);
		return retval;
	}
	printk(KERN_INFO "HELL_YEAH!!1 request_irq() succeed. netdev->irq is %d, adapter->pdev->irq is %d, .\n", netdev->irq, adapter->pdev->irq);

	/* выделяем DMA память под входной буфер*/
	adapter->tx_bufs = pci_alloc_consistent(adapter->pdev, TOTAL_TX_BUF_SIZE, &adapter->tx_bufs_dma);
	/* выделяем DMA память под выходной буфер*/
	adapter->rx_ring = pci_alloc_consistent(adapter->pdev, TOTAL_RX_BUF_SIZE, &adapter->rx_ring_dma);

	if((!adapter->tx_bufs)  || (!adapter->rx_ring))
	{
		printk(KERN_INFO "FAILED to allocate buffer for receive or xmit packets... UR LAME!!1 .\n");
		free_irq(adapter->pdev->irq, netdev);

		if(adapter->tx_bufs) {
			pci_free_consistent(adapter->pdev, TOTAL_TX_BUF_SIZE, adapter->tx_bufs, adapter->tx_bufs_dma);
			adapter->tx_bufs = NULL;
		}
		if(adapter->rx_ring) {
			pci_free_consistent(adapter->pdev, TOTAL_RX_BUF_SIZE, adapter->rx_ring, adapter->rx_ring_dma);
			adapter->rx_ring = NULL;
		}
		return -ENOMEM;
	}
	printk(KERN_INFO "HELL_YEAH!!1 to allocate buffer for xmit packets! pci_alloc_consistent() HELL_YEAH!!1. adapter->tx_bufs is %p.\n", adapter->tx_bufs);

	// Инитим входной кольцевой буфер
	KUGG_init_ring(netdev);
	// Инитиим знаения струкуры
	KUGG_hw_start(netdev);
	return 0;
}

//заглушка
static int KUGG_close(struct net_device *netdev)
{
	printk(KERN_INFO "calls KUGG_close().\n");
	return 0;
}

//"формируем" кадр
static netdev_tx_t KUGG_xmit_frame(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct KUGG_adapter *adapter = netdev_priv(netdev);
	unsigned int entry = adapter->cur_tx;

	skb_copy_and_csum_dev(skb, adapter->tx_buf[entry]); // кладём из буфера по указателю дату в алаптер и ситаем котрольную сумму 
	dev_kfree_skb(skb); //убираем за собой

	entry++;
	adapter->cur_tx = entry % NUM_TX_DESC;

 	if(adapter->cur_tx == adapter->dirty_tx) {
 		netif_stop_queue(netdev);
	}

	return NETDEV_TX_OK; // ошибки надо мочь предусматривать НО-О-О... Константа...
}

//"ООП" на голом СИ - нормальная практика...
static struct net_device_stats *KUGG_get_stats(struct net_device *netdev)
{
	struct KUGG_adapter *adapter = netdev_priv(netdev);

	printk(KERN_INFO "calls KUGG_get_stats().\n");
	return &(adapter->stats);
}

//"методы" - мы хотим эти
static const struct net_device_ops KUGG_netdev_ops = {
	.ndo_open		= KUGG_open,
	.ndo_stop		= KUGG_close,
	.ndo_start_xmit	= KUGG_xmit_frame,
	.ndo_get_stats		= KUGG_get_stats,
};

/* из init.h мы хотим это только во время инициализации. Мы хотим перехватывать здесь ошибки*/
static int __devinit KUGG_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct KUGG_adapter *adapter; // adapter private data structure
	unsigned long mmio_start, mmio_end, mmio_len, mmio_flags; // PCI bus physical mmio address
	void *ioaddr; // virtual address of mmio_start after ioremap()

	//отдельно, зная устройство, мы хотели бы регистр ошибок почитать по-умолчанию. Обычно там много интересного бывает 
	int err = 0, i;

	printk(KERN_INFO "insmod calls KUGG_probe(), Vender ID is 0x%x, Device ID is 0x%x.\n", KUGG_pci_tbl->vendor, KUGG_pci_tbl->device);

	/* enable device (incl. PCI PM wakeup and hotplug setup) */
	err = pci_enable_device_mem(pdev);
	if (err) 
	{
		dev_err(&pdev->dev, "cannot enable PCI device\n");
		printk(KERN_INFO "cannot enable PCI device.\n");
		return err;
	}

	if ((pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0) ||
	    (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)) 
	{
		dev_err(&pdev->dev, "No usable DMA configuration,aborting\n"); //вот бы это был RTOS с гарантиями предоставления ресуров... Можно это по-разному сделать.
		pci_disable_device(pdev);
		return err;
	}

	err = pci_request_regions(pdev, KUGG_driver_name);
	if (err) 
	{
		dev_err(&pdev->dev, "cannot obtain PCI resources\n");
		printk(KERN_INFO "cannot obtain PCI resources.\n");
		pci_disable_device(pdev);
		return err;
	}

	printk(KERN_INFO "Now can obtain PCI resources. pdev->irq is %d.\n", pdev->irq);
	pci_set_master(pdev);// даём PCI шине порулить.
	printk(KERN_INFO "Now pci_set_master.\n"); // no ethernet interface, no /sys/class/net/, yes at /sys/bus/pci/drivers/


	// Инитим в области etherdev память под устройство.
	// устанавливает \/ несколько полей в net_device в соответствующие значения для устройств Ethernet. 
	netdev = alloc_etherdev(sizeof(struct KUGG_adapter));
	if (netdev == NULL) 
	{
		err = -ENOMEM; //отрицательные с ошибкой...
		printk(KERN_INFO "cannot allocate ethernet device resources.\n");
		pci_release_regions(pdev);
		return err;
	}

// set net device's parent device to PCI device - ю ноу...
	SET_NETDEV_DEV(netdev, &pdev->dev);
	pci_set_drvdata(pdev, netdev);

// указатель на дату нетдевайса через "сеттер"
	adapter = netdev_priv(netdev);
// set PCI
	adapter->pdev = pdev;
// set net device
	adapter->netdev = netdev;
// инитим спинлок, хотя в кольцебуферах можно и через атомики.
	spin_lock_init (&adapter->lock);
// get mem map io address
	mmio_start = pci_resource_start(pdev, 0);
	printk(KERN_INFO "mmio_start is 0x%lx.\n", mmio_start); // "mmio_start is 0xf1000000." correct
        mmio_end = pci_resource_end(pdev, 0);
	printk(KERN_INFO "mmio_end is 0x%lx.\n", mmio_end); 
        mmio_len = pci_resource_len(pdev, 0);
	printk(KERN_INFO "mmio_len is 0x%lx.\n", mmio_len); 
        mmio_flags = pci_resource_flags(pdev, 0);
	printk(KERN_INFO "mmio_flags is 0x%lx.\n", mmio_flags); 


	/* ioremap MMI/O region */
	ioaddr = ioremap(mmio_start, mmio_len);
	if(!ioaddr) 
	{
		printk(KERN_INFO "Could not ioremap.\n"); 
		free_netdev(netdev);
	}
	/* set private data */
	netdev->base_addr = (long)ioaddr;
	adapter->hw_addr = ioaddr;
	adapter->regs_len = mmio_len;
	printk(KERN_INFO "Set private data.\n");

	for(i = 0; i < 6; i++) 
	{  
		/* физический адрес через офсет */
		netdev->dev_addr[i] = readb((const volatile void*)(netdev->base_addr+REG_MAC_STA_ADDR+i));
		netdev->broadcast[i] = 0xff;
	}
	printk(KERN_INFO "Found the MAC is %pM.\n", netdev->dev_addr); 
	netdev->hard_header_len = 14; // хочу 14

	// назовём драйвер и интеррапшены
	memcpy(netdev->name, KUGG_driver_name, sizeof(KUGG_driver_name)); 
	printk(KERN_INFO "The IRQ number is %d.\n", adapter->pdev->irq); 
 	netdev->netdev_ops = &KUGG_netdev_ops;

 	/* register the network device */
	err = register_netdev(netdev);
	if (err) 
	{
		printk(KERN_INFO "Could not register netdevice... LAAAAME!.\n");
		return err;
	}

	printk(KERN_INFO "Now init net device in probe function.\n");
	return 0;
}

static void __devexit KUGG_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct KUGG_adapter *adapter = netdev_priv(netdev);

	// rmmod это вызовет
	free_irq(adapter->pdev->irq, netdev);
	iounmap(adapter->hw_addr);
 	unregister_netdev(netdev);
	free_netdev(netdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static struct pci_driver KUGG_driver = {
	.name     = KUGG_driver_name,
	.id_table = KUGG_pci_tbl,
	.probe    = KUGG_probe,
	.remove   = __devexit_p(KUGG_remove),
};


static int __init KUGG_init_module(void)
{	// There is no printf in kernel. 
	printk(KERN_INFO "Init KUGG network driver.\n");
	return pci_register_driver(&KUGG_driver);
}

static void __exit KUGG_exit_module(void)
{
	printk(KERN_INFO "Cleanup KUGG network driver.\n");
	pci_unregister_driver(&KUGG_driver);
}

// обозначь себя 
module_init(KUGG_init_module);

// убери за собой
module_exit(KUGG_exit_module);
