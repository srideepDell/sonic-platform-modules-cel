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
#define NUM_PSU 2
#define UPDATE_MS 100
#define CHECK_MS 1000
#define CHECK_COUNTER  CHECK_MS/UPDATE_MS


struct fan_gpio {
    unsigned char presence_pin;
    unsigned char red_led_pin;
    unsigned char yel_led_pin;
    int prs;
};

struct psu_gpio {
    unsigned char absence_pin;
    unsigned char powerok_pin;
};

static int count;
static struct workqueue_struct *dx010_fan_led_workqueue;
static struct delayed_work fan_update;
static struct gpio_chip *gc;
static struct fan_gpio fan_gpios[NUM_FAN] = {
  {.presence_pin = 10,
   .red_led_pin = 29,
   .yel_led_pin = 30,
   .prs = 1
  },
  {.presence_pin = 11,
   .red_led_pin = 31,
   .yel_led_pin = 32,
   .prs = 1
  },
  {.presence_pin = 12,
   .red_led_pin = 33,
   .yel_led_pin = 34,
   .prs = 1
  },
  {.presence_pin = 13,
   .red_led_pin = 35,
   .yel_led_pin = 36,
   .prs = 1
  },
  {.presence_pin = 14,
   .red_led_pin = 37,
   .yel_led_pin = 38,
   .prs = 1
  }
};

static struct psu_gpio psu_gpios[NUM_PSU] = {
  {.absence_pin = 27,
   .powerok_pin = 22
  },
  {.absence_pin = 28,
   .powerok_pin = 25,
  }
};

static int chip_match_name(struct gpio_chip *chip, void *data){
  return !strcmp(chip->label, data);
}

static void init_fan_gpio(void){
  int index;
  for( index=0; index < NUM_FAN; index++){
    gpio_direction_input(gc->base+fan_gpios[index].presence_pin);
    gpio_direction_output(gc->base+fan_gpios[index].red_led_pin, 1);
    gpio_direction_output(gc->base+fan_gpios[index].yel_led_pin, 0);
  }
}

static void init_psu_gpio(void){
  int index;
  for( index=0; index < NUM_PSU; index++){
    gpio_request(gc->base+psu_gpios[index].absence_pin, NULL);
    gpio_request(gc->base+psu_gpios[index].powerok_pin, NULL);
    gpio_direction_input(gc->base+psu_gpios[index].absence_pin);
    gpio_direction_input(gc->base+psu_gpios[index].powerok_pin);
    gpio_export(gc->base+psu_gpios[index].absence_pin,0);
    gpio_export(gc->base+psu_gpios[index].powerok_pin,0);
  }
}

static void update_led(void){
  int index;
  int presence;
  int delay_ms;
  struct gpio_chip *find;

  /* Check for gpio chip exist every CHECK_MS */
  if(count <= 0){
    find = gpiochip_find("pca9505", chip_match_name);
    if(find){
      /* Skip check routine, Update the leds */
      if(gc != find){
        gc = find;
        init_fan_gpio();
        init_psu_gpio();
      }
      count = CHECK_COUNTER;
      delay_ms = UPDATE_MS;
    }else{
      /* Put the check routine to the queue */
      delay_ms = CHECK_MS;
    }
  }else{
    for (index=0; index < NUM_FAN; index++){
      presence = !gpio_get_value_cansleep(gc->base+fan_gpios[index].presence_pin);
      if( presence != fan_gpios[index].prs ){
        if( presence ){
          gpio_set_value_cansleep(gc->base+fan_gpios[index].red_led_pin, 1);
          gpio_set_value_cansleep(gc->base+fan_gpios[index].yel_led_pin, 0);
        }else{
          gpio_set_value_cansleep(gc->base+fan_gpios[index].red_led_pin, 0);
          gpio_set_value_cansleep(gc->base+fan_gpios[index].yel_led_pin, 1);
        }
      }
      fan_gpios[index].prs = presence;
    }
    count--;
  }
  queue_delayed_work(dx010_fan_led_workqueue, &fan_update,
                     msecs_to_jiffies(delay_ms));
}

static void deinit_psu_gpio(void){
  int index;
  for( index=0; index < NUM_PSU; index++){
    gpio_unexport(gc->base+psu_gpios[index].absence_pin);
    gpio_unexport(gc->base+psu_gpios[index].powerok_pin);
    gpio_free(gc->base+psu_gpios[index].absence_pin);
    gpio_free(gc->base+psu_gpios[index].powerok_pin);
  }
}

static int __init dx010_fan_led_init(void){
  count = 0;
  gc = NULL;
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
  cancel_delayed_work_sync(&fan_update);
  destroy_workqueue(dx010_fan_led_workqueue);
  deinit_psu_gpio();
  dx010_fan_led_workqueue = NULL;
  gc = NULL;
}

module_init(dx010_fan_led_init);
module_exit(dx010_fan_led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Celestica Inc.");
MODULE_DESCRIPTION("DX010 fan led control");