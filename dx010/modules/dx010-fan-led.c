#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/printk.h>

#define MODULE_NAME "dx010-fan-led"

#define NUM_FAN 5
#define UPDATE_MS 10


struct fan_gpio {
    unsigned char present_pin;
    unsigned char red_led_pin;
    unsigned char yel_led_pin;
    int prs;
};

static struct workqueue_struct *dx010_fan_led_workqueue;
static struct delayed_work fan_update;
static struct gpio_chip *gc;
static struct fan_gpio fan_gpios[NUM_FAN] = {
    {.present_pin = 10,
     .red_led_pin = 29,
     .yel_led_pin = 30,
     .prs = 1
    },
    {.present_pin = 11,
     .red_led_pin = 31,
     .yel_led_pin = 32,
     .prs = 1
    },
    {.present_pin = 12,
     .red_led_pin = 33,
     .yel_led_pin = 34,
     .prs = 1
    },
    {.present_pin = 13,
     .red_led_pin = 35,
     .yel_led_pin = 36,
     .prs = 1
    },
    {.present_pin = 14,
     .red_led_pin = 37,
     .yel_led_pin = 38,
     .prs = 1
    }
};

static int chip_match_name(struct gpio_chip *chip, void *data){
  return !strcmp(chip->label, data);
}

static void init_led(void){
  int index;
  for( index=0; index < NUM_FAN; index++){
    gpio_direction_input(gc->base+fan_gpios[index].present_pin);
    gpio_direction_output(gc->base+fan_gpios[index].red_led_pin, 1);
    gpio_direction_output(gc->base+fan_gpios[index].yel_led_pin, 0);
  }
}

static void update_led(void){
  int index;
  int present;

  for (index=0; index < NUM_FAN; index++){
    present = !gpio_get_value(gc->base+fan_gpios[index].present_pin);
    if( present != fan_gpios[index].prs ){
      if( present ){
        gpio_set_value(gc->base+fan_gpios[index].red_led_pin, 1);
        gpio_set_value(gc->base+fan_gpios[index].yel_led_pin, 0);
      }else{
        gpio_set_value(gc->base+fan_gpios[index].red_led_pin, 0);
        gpio_set_value(gc->base+fan_gpios[index].yel_led_pin, 1);
      }
    }
    fan_gpios[index].prs = present;
  }
  queue_delayed_work(dx010_fan_led_workqueue, &fan_update,
                     msecs_to_jiffies(UPDATE_MS));
}

static int __init dx010_fan_led_init(void){
  // TODO:
  // 1. Add two work types for gc check and fan led update.
  // 2. Extend support to PSUs gpios too.
  gc = gpiochip_find("pca9505", chip_match_name);
  if(!gc){
    printk(KERN_INFO "GPIO chip not found!\n");
    return -ENXIO;
  }
  init_led();
  dx010_fan_led_workqueue = create_singlethread_workqueue(MODULE_NAME);
  if (IS_ERR(dx010_fan_led_workqueue)) {
    printk(KERN_INFO "failed to inittialize workqueue\n");
    return PTR_ERR(dx010_fan_led_workqueue);
  }

  INIT_DELAYED_WORK(&fan_update, update_led);
  queue_delayed_work(dx010_fan_led_workqueue, &fan_update,
                     msecs_to_jiffies(UPDATE_MS));
  return 0;
}

static void __exit dx010_fan_led_exit(void){
  destroy_workqueue(dx010_fan_led_workqueue);
  gc = NULL;
  dx010_fan_led_workqueue = NULL;
}

module_init(dx010_fan_led_init);
module_exit(dx010_fan_led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Celestica Inc.");
MODULE_DESCRIPTION("DX010 fan led control");