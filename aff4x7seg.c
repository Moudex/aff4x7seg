#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
//#include <linux/slab.h> // kmalloc()

#define AFF_NAME "aff4x7seg"
#define AFF_MAX_LEN (12)

#define BROCHE_SEG_A (23)
#define BROCHE_SEG_B (24)
#define BROCHE_SEG_C (25)
#define BROCHE_SEG_D (5)
#define BROCHE_SEG_E (6)
#define BROCHE_SEG_F (12)
#define BROCHE_SEG_G (13)
#define BROCHE_SEG_P (19)
#define BROCHE_DIGIT_1 (4)
#define BROCHE_DIGIT_2 (17)
#define BROCHE_DIGIT_3 (27)
#define BROCHE_DIGIT_4 (22)
#define BROCHE_DIGIT_5 (18)

MODULE_AUTHOR("Jeremi DUPIN");
MODULE_DESCRIPTION("Gestion d'un afficheur 4x7 segments branche sur les GPIOs");
MODULE_SUPPORTED_DEVICE("LTC-4727");
MODULE_LICENSE("GPL");
static int major = 240;
module_param(major, int, 0);
MODULE_PARM_DESC(major, "major number");

struct caract {
    char symb;
    char seg[7];
};

/** Segment de l'affichage */
static unsigned int segments[] = {
    BROCHE_SEG_A,
    BROCHE_SEG_B,
    BROCHE_SEG_C,
    BROCHE_SEG_D,
    BROCHE_SEG_E,
    BROCHE_SEG_F,
    BROCHE_SEG_G,
    BROCHE_SEG_P
};

/** Digits de l'affichage */
static unsigned int digits[] = {
    BROCHE_DIGIT_1,
    BROCHE_DIGIT_2,
    BROCHE_DIGIT_3,
    BROCHE_DIGIT_4,
    BROCHE_DIGIT_5
};

/** Table des caractères pris en charge */
static const struct caract aff_caracteres[] = {
    {'0', {1,1,1,1,1,1,0}},
    {'1', {0,1,1,0,0,0,0}},
    {'2', {1,1,0,1,1,0,1}},
    {'3', {1,1,1,1,0,0,1}},
    {'4', {0,1,1,0,0,1,1}},
    {'5', {1,0,1,1,0,1,1}},
    {'6', {1,0,1,1,1,1,1}},
    {'7', {1,1,1,0,0,0,0}},
    {'8', {1,1,1,1,1,1,1}},
    {'9', {1,1,1,1,0,1,1}},
    {'A', {1,1,1,0,1,1,1}},
    {'B', {0,0,1,1,1,1,1}},
    {'C', {1,0,0,1,1,1,0}},
    {'D', {0,1,1,1,1,0,1}},
    {'E', {1,0,0,1,1,1,1}},
    {'F', {1,0,0,0,1,1,1}},
    {'G', {1,0,1,1,1,1,0}},
    {'H', {0,1,1,0,1,1,1}},
    {'I', {0,1,1,0,0,0,0}},
    {'J', {1,1,1,1,0,0,0}},
    {'L', {0,0,0,1,1,1,0}},
    {'N', {0,0,1,0,1,0,1}},
    {'O', {0,0,1,1,1,0,1}},
    {'P', {1,1,0,0,1,1,1}},
    {'Q', {1,1,1,0,0,1,1}},
    {'S', {1,0,1,1,0,1,1}},                                                 
    {'U', {0,1,1,1,1,1,0}},
    {'Y', {0,1,1,1,0,1,1}},
    {'_', {0,0,0,1,0,0,0}},
    {' ', {0,0,0,0,0,0,0}},
    {'\0', {0,0,0,0,0,0,0}}
};
static char aff_mess[AFF_MAX_LEN];	// Message affiché
static int aff_len = 0;			// Taille du message affiché
static DEFINE_SPINLOCK(aff_lock);	// Verrou tournant pour l'affichage
static struct task_struct *aff_thr;	// Thread d'affichage

/**
 * aff_app - thread d'affichage
 */
static int aff_app(void *data)
{
    char buf[AFF_MAX_LEN];
    int buff_len, g;
    unsigned long flags;

    while (!kthread_should_stop()) {
	int i = 0;
	int pos = 0;
	int dig = 0;
	int d5[] = {0,0,0};
	/* Copie du message */
	// SECTION CRITIQUE
	spin_lock_irqsave(&aff_lock, flags);
	//printk(KERN_DEBUG "%s: thread: Section critique, message \"%s\"\n", THIS_MODULE->name, aff_mess);
	while (aff_mess[i] != '\0') {
	    buf[i] = aff_mess[i];
	    i++;
	}
	buf[i] = '\0';
	buff_len = aff_len;
	spin_unlock_irqrestore(&aff_lock, flags);
	// FIN SECTION CRITIQUE

	/* Affichage du message */
	while (dig < 5 && buf[pos] != '\0') {
	    /* Prise en charge de ":" */
	    if (dig == 2 && buf[pos] == ':') {
		d5[0] = 1; d5[1] = 1;
		pos++;
		continue;
	    }

	    /* Prise en charge de "°" */
	    if (dig == 3 && buf[pos] == 194 && buf[pos+1] == 176) {
		d5[2] = 1;
		pos += 2;
		continue;
	    }

	    /* Affichage de ":" et "°" */
	    if (dig == 4) {
		for (i=0; i<3; i++)
		    gpio_set_value(segments[i], d5[i]);
	    }
	    /* Affichage du caractère */
	    else {
		int j;
		for (j=0; aff_caracteres[j].symb != buf[pos]; j++)
		    /* Rien :) */ ;
		for (i=0; i<7; i++)
		    gpio_set_value(segments[i], aff_caracteres[j].seg[i]);
		pos++;
		if (buf[pos] == '.') {
		    gpio_set_value(BROCHE_SEG_P, 1);
		    pos++;
		} else { gpio_set_value(BROCHE_SEG_P, 0); }
	    }

	    /* Activation du digit */
	    gpio_set_value(digits[dig], 1);
	    //current->state = TASK_INTERRUPTIBLE;
	    //schedule_timeout(1);
	    usleep_range(250, 350);
	    gpio_set_value(digits[dig], 0);
	    dig++;
	}
	//current->state = TASK_INTERRUPTIBLE;
	//schedule_timeout(1);
	usleep_range(250, 350);
    }

    /* Arret du thread */
    /* Extinction de l'affichage */
    for (g=0; g<8; g++)
	gpio_set_value(segments[g], 0);
    for (g=0; g<5; g++)
	gpio_set_value(digits[g], 0);

    //printk(KERN_DEBUG "%s: thread: arret\n", THIS_MODULE->name);
    return 0;
}

/**
 * aff_is_char - vérifie si le caractère est autorisé
 * @c: caractère à vérifier
 */
static int aff_is_char(char c)
{
    int i;
    for (i=0; aff_caracteres[i].symb != '\0'; i++)
	if (aff_caracteres[i].symb == c)
	    return 1;
    return 0;
}

/**
 * aff_mess_valide - vérifie la validité du message
 * @buf: message à vérifier
 */
static int aff_mess_valide(char *buf)
{
    int pos = 0, dig = 0;
    while (dig < 4 && buf[pos] != '\0') {
	/* Prise en charge de ":" */
	if (dig == 2 && buf[pos] == ':') {
	    pos++;
	    continue;
	}

	/* Prise en charge de "°" */
	if (dig == 3 && buf[pos] == 194 && buf[pos+1] == 176) {
	    pos += 2;
	    continue;
	}

	/* Caractère valide ? */
	if (aff_is_char(buf[pos])) pos++;
	else return 0;

	/* Prise en charge de "." */
	if (buf[pos] == '\0') break;
	if (buf[pos] == '.') pos++;
	
	dig++;
    }
    if (buf[pos] == '\0')
	return 1;
    return 0;
}

static ssize_t aff_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    char output[AFF_MAX_LEN];
    unsigned long flags;
    int i = 0;
    //printk(KERN_DEBUG "%s: read: demande de lecture de %d octets\n", THIS_MODULE->name, count);

    // SECTION CRITIQUE
    spin_lock_irqsave(&aff_lock, flags);
    while (aff_mess[i] != '\0') {
	output[i] = aff_mess[i];
	i++;
    }
    output[i] = '\n'; i++; output[i] = '\0';
    i = aff_len + 1;
    spin_unlock_irqrestore(&aff_lock, flags);
    // FIN SECTION CRITIQUE

    if (*ppos >= aff_len+1) {
	//printk(KERN_DEBUG "%s: read: fin du fichier\n", THIS_MODULE->name);
	return 0;
    }
    if (*ppos + count > i)
	count = i - *ppos;

    if (count) {
	if (copy_to_user(buf, &output[*ppos], count)) {
	    printk(KERN_WARNING "%s: read: erreur de copy_to_user()\n", THIS_MODULE->name);
	    return -EFAULT;
	}
    }
    *ppos += count;

    //printk(KERN_DEBUG "%s: read: %d octets reelement lus\n", THIS_MODULE->name, count);

    return count;
}

static ssize_t aff_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    char tmp_buffer[AFF_MAX_LEN];
    //printk(KERN_DEBUG "%s: write: demande d'ecriture de %d octets\n", THIS_MODULE->name, count);

    /* check for overflow */
    if (count > AFF_MAX_LEN) {
	printk(KERN_WARNING "%s: write: trop de donnes a ecrire: %do\n", THIS_MODULE->name, count);
	return -EFAULT;
    }

    if (copy_from_user(tmp_buffer, buf, count)) {
	//printk(KERN_DEBUG "%s: write: echec de copy_from_user\n", THIS_MODULE->name);
	return -EFAULT;
    }

    tmp_buffer[count-1] = '\0';

    /* Check for validity */
    if (aff_mess_valide(tmp_buffer)) {
	unsigned long flags;
	// SECTION CRITIQUE
	spin_lock_irqsave(&aff_lock, flags);
	aff_len = count;
	if (copy_from_user(aff_mess, buf, count)) {
	    write_unlock(&aff_lock);
	    //printk(KERN_DEBUG "%s: write: echec de copy_from_user dans la section critique\n", THIS_MODULE->name);
	    return -EFAULT;
	}
	aff_mess[count-1] = '\0';
	spin_unlock_irqrestore(&aff_lock, flags);
	// FIN SECTION CRITIQUE
	return count;
    }
    printk(KERN_WARNING "%s: write: message invalide: \"%s\"\n", THIS_MODULE->name, tmp_buffer);
    return -EFAULT;
}

static loff_t aff_llseek(struct file *file, loff_t offset, int mode)
{
    /* Recherche illégale */
    printk(KERN_WARNING "%s: recherche illegale\n", THIS_MODULE->name);
    return -ESPIPE;
}

static int aff_open(struct inode *inode, struct file *file)
{
    //printk(KERN_DEBUG "%s: open: message \"%s\"\n", THIS_MODULE->name, aff_mess);
    return 0;
}

static int aff_release(struct inode *inode, struct file *file)
{
    //printk(KERN_DEBUG "%s: close: message \"%s\"\n", THIS_MODULE->name, aff_mess);
    return 0;
}

static struct file_operations fops =
{
    .read = aff_read,
    .write =  aff_write,
    .open = aff_open,
    .release = aff_release,
    .llseek = aff_llseek
};

static int __init aff_init(void)
{
    int ret;
    int i;
    char *label_seg = "SEG__";
    char *label_dig = "DIG__";

    /* Enregistrement du driver */
    ret = register_chrdev(major, AFF_NAME, &fops);
    if (ret < 0) {
	printk(KERN_WARNING "%s: probleme sur le major\n", THIS_MODULE->name);
	return ret;
    }

    /* Configuration des GPIO */
    // Segments
    for (i=0; i<8; i++) {
	if (i==7) label_seg[4] = 'P';
	else label_seg[4] = 'A'+i;
	gpio_request(segments[i], label_seg);
	gpio_direction_output(segments[i], 0);
    }
    // Digits
    for (i=0; i<5; i++) {
	label_dig[4] = '1'+i;
	gpio_request(digits[i], label_dig);
	gpio_direction_output(digits[i], 0);
    }

    /* Démarrage du thread */
    aff_thr = kthread_run(aff_app, NULL, "aff4x7_print");

    //printk(KERN_DEBUG "%s: charge avec succes\n", THIS_MODULE->name);
    return 0;
}

static void __exit aff_cleanup(void)
{
    int i;

    /* Arret du thread */
    kthread_stop(aff_thr);

    /* Libération des GPIO */
    for (i=0; i<8; i++)
	gpio_free(segments[i]);
    for (i=0; i<5; i++)
	gpio_free(digits[i]);

    /* Supression de l'enregistrement */
    unregister_chrdev(major, AFF_NAME);
    //printk(KERN_DEBUG "%s: decharge avec succes\n", THIS_MODULE->name);
}

module_init(aff_init);
module_exit(aff_cleanup);
