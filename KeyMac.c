#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <net/inetpeer.h>

#define LOG_PRE KBUILD_MODNAME
#define HPRINTK(fmt,args...) printk(KERN_DEBUG "%s %s: " fmt,LOG_PRE,__FUNCTION__,args)
#define DPRINTK(fmt,args...) printk(KERN_INFO "%s %s: " fmt,LOG_PRE,__FUNCTION__,args)
#define DEVICE_NAME "KeyMac"
#define PROCFS_MAX_SIZE 1000

MODULE_DESCRIPTION("Keyboard Macro Recorder");
MODULE_LICENSE("GPL");

static struct input_dev *keymac_dev;
static struct proc_dir_entry *keymac_proc;
static int macros_started = 0;
static int macros_count = 0;
static int i = 0;
static int j = 0;
static int cas = 0;
static int last_three_pressed[3] = {-1, -1, -1};
static int var = -1;
char proc_buf[PROCFS_MAX_SIZE];

/**
* struct node - A node for linked list
* @code:		Scancode of the key
* @status:		Status denotes if a key is pressed or released
*				pressed  - 1
*				released - 0
* @next:		Pointer to next node
* 
* node is used for storing macros in a linked list as their length
* can be unknown. 
*/
static struct node
{
	int code;
	int status;
	struct node *next;
};

/**
* struct Macros - A mapping between macro and key combination to start the macro
* @len:			  Length of macro
* @identifier:	  An array of length 3 which stores scancodes of 3 keys which identifies 
*				  this macro and is used to start the macro
* @macro:		  A linked list which stores the macro
*
* Macros is a mapping between a set of keys to identify / start a particular macro.
* An array of mapping is used to store multiple macros.
*/
static struct Macros 
{
	int len;
	int identifier[3];
	struct node *macro;
} macros_map[10];


/**********************************************
*			Linked List Operations 			  *
***********************************************/

/**
* push() - Pushes a new node in linked list
* @head:   Start of linked list on which a new node is to be pushed
* @code:   Scancode of key
* @status: Status of key being pressed or released
*
* Pushes a new key with its status in the linked list
*/
static void push(struct node **head, int code, int pressed)
{
	struct node *new_node = kzalloc(sizeof(struct node), GFP_KERNEL);
	new_node->code = code;
	new_node->status = pressed;
	new_node->next = *head;
	*head = new_node;
};

/**
* pop() - Pops the starting of linked list
* @head:  Pointer to a pointer of node from which node is to be popped.
*/
static void pop(struct node **head) {
	if((*head) == NULL)
	{
		return;
	}
	struct node *temp = *head;
	(*head) = (*head) -> next;
	kfree(temp);
}

/**
* reverse_stack() - Reverses a linked list
* @head:			Head of linked list which is to be reversed
*
* As macro is stored in reverse order, it must be reversed for
* proper execution of macro
*/
static void reverse_stack(struct node **head) 
{
	struct node *prev = NULL;
	struct node *curr = *head;
	struct node *next = NULL;
	while(curr != NULL)
	{
		next = curr->next;
		curr->next = prev;
		prev = curr;
		curr = next;
	}
	*head = prev;
}

/**********************************************
*			   Macro Operations 		  	  *
***********************************************/

/**
* number_to_keycode() - Converts integer into the scancode of corresponding numeric key
* @n:					Integer in range 0 to 9
*
* Return: 				Scancode of corresponding numeric key
*/
static int number_to_keycode(int n) 
{
	switch(n)
	{
		case 1: return KEY_1;
		case 2: return KEY_2;
		case 3: return KEY_3;
		case 4: return KEY_4;
		case 5: return KEY_5;
		case 6: return KEY_6;
		case 7: return KEY_7;
		case 8: return KEY_8;
		case 9: return KEY_9;
		case 0: return KEY_0;
	}
};

/**
* add_to_macro() - Adds a key and its status in macro
* @code:		   Scancode of key to be added in macro
* @pressed:		   Status of key being pressed or released
*/
static void add_to_macro(int code, int pressed) 
{
	push(&(macros_map[macros_count].macro), code, pressed);
	macros_map[macros_count].len++;
}

/**
* delete_macro() - Deletes a linked list
* @head:		   Head of a linked list which is to be deleted
*/
static void delete_macro(struct node **head)
{
	while((*head)) 
	{
		pop(head);
	}
}

/**
* fill_last_three_pressed() - Sets corresponding key to -1 if already present in last_three_pressed array
*							  or adds it in last_three_pressed array
* @code:		   			  Scancode of key pressed / released
* @pressed:		   			  Status of key being pressed or released
*
* This function modifies global array last_three_pressed which stores the last three values pressed 
* in an array. Note if the key is released, its value in array is again set to -1. So last_three_pressed
* stores scancodes three currently pressed keys.
*/
static void fill_last_three_pressed(int code, int pressed)
{
	if(pressed == 1) 
	{
		for(i = 0; i < 3; i++) 
		{
			if(last_three_pressed[i] == -1)
			{
				last_three_pressed[i] = code;
				break;
			}
		}
	}
	else if(pressed == 0)
	{
		for(i = 0; i < 3; i++) 
		{
			if(last_three_pressed[i] == code)
			{
				last_three_pressed[i] = -1;
				break;
			}
		}
	}
};

/**
* special_combination() - Checks if the last_three_pressed values matches with any 
* 						  of the identifier or not
* 
* Return:
* -3 - No key pressed
* -1 - Some key is pressed but not matches with any identifier
* i  - value between 0 and 9 which is the corresponding index in macro_map whose 
*      identifier matches with last_three_pressed 
*/
static int special_combination(void)
{
	int count = 0;

	/* no key pressed */
	if(last_three_pressed[0] + last_three_pressed[1] + last_three_pressed[2] == -3)
	{
		return -3;
	}

	/* for start and stop of recording */
	for(i = 0; i < 3; i++) {
		if(last_three_pressed[i] == KEY_LEFTCTRL 
			|| last_three_pressed[i] == KEY_LEFTALT
			|| last_three_pressed[i] == KEY_LEFTSHIFT)
		{
			count++;
		}
	}

	if(count == 3)
	{
		return 10;
	}

	for(i = 0; i < 10; i++)
	{
		count = 0;
		for(j = 0; j < 3; j++) 
		{
			if(last_three_pressed[j] == macros_map[i].identifier[0] 
			|| last_three_pressed[j] == macros_map[i].identifier[1]
			|| last_three_pressed[j] == macros_map[i].identifier[2])
			{
				count++;
			}
		}
		if(count == 3)
			return i;
	}
	return -1;
};

/**
* execute_macro() - Executes a macro using input_report_key
* @macro_number:	The index of macro_map to be executed
*
* This function traverses the linked list stored in macro_map[macro_number].macro and
* reports each key using input_report_key function
*/
static void execute_macro(int macro_number)
{
	var = -1;
	struct node *trav = macros_map[macro_number].macro;
	int ct = 0;

	while(trav != NULL)
	{
		printk(KERN_INFO "Playing Key : %x Status : %d\n", trav->code, trav->status);
		input_report_key(keymac_dev, trav->code, trav->status);
		if(trav->status)
			ct += 1;
		else
			ct -= 1;
		if(ct == 0)
			input_sync(keymac_dev);
		trav = trav->next;
	}
}

/**
* keystore() - Called on each key press. This function is responsible for start / stop of recording 
* 			   of macros and executing macros based on keys pressed
* @code: 	   Scancode of key pressed / released
* @pressed:	   Status of key being pressed or released
*/
static int keystore(int code, int pressed)
{
	int key_comb = -1;
	fill_last_three_pressed(code, pressed);

	key_comb = special_combination();
	if(key_comb == 10) 
	{
		cas = 1;
		if(macros_started) 
		{
			cas = 0;
			macros_started = 0;
			printk(KERN_INFO "Stopped Recording Macros\n");
			pop(&(macros_map[macros_count].macro));
			pop(&(macros_map[macros_count].macro));
			reverse_stack(&(macros_map[macros_count].macro));
			struct node *trav = macros_map[macros_count].macro;
			while(trav) {
				printk(KERN_INFO "Key : %x Status : %d\n", trav->code, trav->status);
				trav = trav->next;
			}
			macros_map[macros_count].identifier[0] = KEY_LEFTCTRL;
			macros_map[macros_count].identifier[1] = KEY_LEFTSHIFT;
			macros_map[macros_count].identifier[2] = number_to_keycode(macros_count);
			macros_count = (macros_count + 1) % 10;
			return 0;
		}
	}

	else if(cas == 1 && key_comb == -3)
	{
		cas = 0;
		macros_started = 1;
		printk(KERN_INFO "Started Recording Macros\n");
		delete_macro(&(macros_map[macros_count].macro));
		macros_map[macros_count].len = 0;
		macros_map[macros_count].macro = NULL;	
		return 0;
	}

	if(macros_started) 
	{
		add_to_macro(code, pressed);
	}

	if(key_comb >= 0 && key_comb < 10) 
	{
		var = key_comb;
		printk(KERN_INFO "KeyComd %d\n", key_comb);
	}

	if(var != -1 && key_comb == -3) {
		printk(KERN_INFO "KeyComd Run");
		udelay(1000);
		execute_macro(var);
	}

	return 0;
}

/**********************************************
*			   Device Driver &    		  	  *
* 			    Input Handler 				  *
*				  Operations 				  *
***********************************************/


/**
* keymac_filter() - Function called when a event occurs from an input device
					but we only process the event if it is EV_KEY i.e keyboard event
* @handle:			Pointer to input handler
* @type:			Type of event
* @code:			Scancode of key pressed
* @value:			Status of key being pressed or released
*/
static bool keymac_filter(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	/* If type is not a key event then do nothing */
	if (type != EV_KEY)
	{
		return 0;
	}

	/* else call keystore */
	return keystore(code, value);
}

/**
* keymac_connect() - Called when an keymac input device connects
* @handler: 		 Input handler of device
* @dev:				 Input device
* @id:				 Id of input device
*/
static int keymac_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "keymac_handle";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	DPRINTK("Connected device: %s (%s at %s)\n",
		   dev_name(&dev->dev),
		   dev->name ?: "unknown",
		   dev->phys ?: "unknown");

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}


/**
* keymac_disconnect() - This function is called when input device disconnects
* @handle: 				Input handler of device
*/
static void keymac_disconnect(struct input_handle *handle)
{
	DPRINTK("Disconnect %s\n", handle->dev->name);

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static int keymac_open(struct input_dev *dev) 
{	
	return 0; 
}

static void keymac_close(struct input_dev *dev) {}

/**
* keymac_proc_write() - This function is called when /dev/keymac_proc file is written
* 						It updates the macros stored in the buffer.
*
* @fp:					file pointer
* @buf:					buffer in which data was written
* @len:					length of data
* @off:					offset
*
* Return - length of buffer
*/
static ssize_t keymac_proc_write(struct file *fp, const char *buf, size_t len, loff_t * off)
{	
	if(len > PROCFS_MAX_SIZE) 
	{ 
		return -EFAULT; 
	}

	if(copy_from_user(proc_buf, buf, len)) 
	{ 
		return -EFAULT;
	}

	printk(KERN_INFO "%s", proc_buf);
	int i=0, j=0, val = 0, seq_flag = 1, seq_id = 0, index=0;
	int seq[3], is_pressed[10], pressed = 0;
	struct node *data;

	for(i=0; i<10; i++) 
	{
		is_pressed[i] = -1;
		delete_macro(&(macros_map[i].macro));
		macros_map[i].len = 0;
	}

	i=0;
	while(i<len) 
	{
		val = 0;
		while(i < len && proc_buf[i] >= '0' && proc_buf[i] <= '9') 
		{
			val = val*10 + proc_buf[i] - '0';
			i++;
			printk(KERN_INFO "keys--- %d", val);
		}

		if(i < len && proc_buf[i] == ':') 
		{
			seq_flag = 0;
			i++;
			continue;
		}

		if(val == 0) 
		{
			i++;
			continue;
		}
	
		if(seq_flag) 
		{
			macros_map[index].identifier[seq_id] = val;
			seq_id++;
		}

		else 
		{
			pressed = 1;
			for(j=0; j<10; j++) 
			{
				if(is_pressed[j] == val) 
				{
					is_pressed[j] = -1;
					pressed = 0;
					break;
				}
			}

			if(pressed == 1) 
			{
				for(j=0; j<10; j++) 
				{
					if(is_pressed[j] == -1) 
					{
						is_pressed[j] = val;
						break;
					}
				}
			}

			push(&(macros_map[index].macro), val, pressed);
			macros_map[index].len++;

			if(i < len && proc_buf[i] == '\n') 
			{
				index++;
				seq_flag = 1;
				seq_id = 0;
				i++;
			}
		}
   	}

   	macros_count = 0;
	for(i=0; i<10; i++) 
	{
		if(macros_map[i].len) 
		{
			macros_count++;
			reverse_stack(&(macros_map[i].macro));
		}	
   	}

	return len;
}

/**
* keymac_proc_read() - This function is called when /dev/keymac_proc file is read
*					   It copies the macros stored in kernel buffer to user buffer
* @fp:				   File pointer
* @buff:			   buffer in which data is to be written
* @len:				   length 
* off:				   offset
*
* Return - length of buffer
*/
static ssize_t keymac_proc_read(struct file *fp, char *buff, size_t len, loff_t *off) 
{
	static int finished = 0;
	if(finished) 
	{
		finished = 0;
		return 0;
	}
	finished = 1;
	memset(buff, 0, macros_map[0].len * 4 * 10);
	sprintf(buff, "");

	for(i = 0; i < 10; ++i) 
	{
		if(macros_map[i].len == 0)
		{
			continue;
		}

		struct node* trav = macros_map[i].macro;
		char temp[100];
		sprintf(temp, "%d %d %d : ",  macros_map[i].identifier[0], macros_map[i].identifier[1], macros_map[i].identifier[2]);
		strcat(buff, temp);
		while(trav != NULL)
		{
			sprintf(temp, "%d ", trav->code, trav->status);
			strcat(buff, temp);
			trav = trav -> next;
		}
		strcat(buff, "\n");
	}

	printk(KERN_INFO "Length : %d", strlen(buff));
	return strlen(buff);
}

static const struct input_device_id keymac_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },					/* Terminating zero entry */
};

static struct input_handler keymac_handler = {
	.filter 	=	keymac_filter,
	.connect 	=	keymac_connect,
	.disconnect =	keymac_disconnect,
	.name 		=	DEVICE_NAME,
	.id_table 	=	keymac_ids,
};

static struct file_operations keymac_proc_fops = {
	.owner = THIS_MODULE,
	.read  = keymac_proc_read,
	.write = keymac_proc_write,
};

MODULE_DEVICE_TABLE(input, keymac_ids);


/*
* keymac_init() - This function is called when module is loaded into kernel. It registers the input
*				  device driver and input handler.
*/
static int __init keymac_init(void)
{
	int error;
	int div;

	keymac_dev = input_allocate_device();
	if (!keymac_dev)
	{
		DPRINTK("Registering device %s failed (no memory)\n",DEVICE_NAME);
		error = -ENOMEM;
		goto err_exit;
	}

	keymac_dev->name = DEVICE_NAME;
	keymac_dev->phys = "keymac/input0";
	keymac_dev->id.bustype = BUS_VIRTUAL;
	keymac_dev->id.vendor  = 0x0000;
	keymac_dev->id.product = 0x0000;
	keymac_dev->id.version = 0x0000;

	keymac_dev->evbit[0] = BIT_MASK(EV_KEY);
	div = KEY_CNT / BITS_PER_LONG; 
	for(i = 0; i < div; i++) 
	{
		keymac_dev->keybit[i] = (1UL << BITS_PER_LONG)- 1;
	}

	keymac_dev->open  = keymac_open;
	keymac_dev->close = keymac_close;

	error = input_register_device(keymac_dev);
	if (error != 0)
	{
		DPRINTK("Registering %s failed (%d)\n",DEVICE_NAME,error);
		goto err_free_dev;
	}
	else
	{
		DPRINTK("Registered %s successfully\n",DEVICE_NAME);
	}

	error = input_register_handler(&keymac_handler);
	if (error)
	{
		DPRINTK("Registering input handler failed with (%d)\n",error);
		goto err_unregister_dev;
	}

	keymac_proc = proc_create("keymac_proc", 0666, NULL, &keymac_proc_fops);
	if(keymac_proc == NULL) 
	{
		printk(KERN_ALERT "Error: Could not initialize keymac_proc\n");
	}

	return 0;

err_unregister_dev:
	input_unregister_device(keymac_dev);

err_free_dev:
	input_free_device(keymac_dev);

err_exit:
	return error;
}

/**
* keymac_exit() - This function is called when module is removed from kernel
*/
static void __exit keymac_exit(void)
{
	input_unregister_handler(&keymac_handler);
	input_unregister_device(keymac_dev);
	input_free_device(keymac_dev);
	remove_proc_entry("keymac_proc", NULL);
}

module_init(keymac_init);
module_exit(keymac_exit);

