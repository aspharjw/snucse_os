#include <linux/rotation.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <uapi/asm-generic/errno-base.h>

int _degree;	// current degree
DEFINE_SPINLOCK(degree_lock);

int read_locked[360];
int write_locked[360];
DEFINE_SPINLOCK(locker);

DECLARE_WAIT_QUEUE_HEAD(read_q);
DECLARE_WAIT_QUEUE_HEAD(write_q);

int sys_set_rotation(int degree) {
	spin_lock(&degree_lock);
	if( degree <0 || degree >= 360) {
			spin_unlock(&degree_lock);
			return -EINVAL;
	}
	_degree = degree;
	spin_unlock(&degree_lock);
	printk(KERN_DEBUG "set_rotation to %d\n", _degree);

	wake_up(&write_q);
//	printk(KERN_DEBUG "wake up all write lockers\n");
	wake_up(&read_q);
//	printk(KERN_DEBUG "wake up all read lockers\n");

	return 1;
}

int convertDegree(int n) {
	if(n < 0) return n + 360;
	if(n >=360) return n - 360;
	return n;
}


int isInRange(int now, int degree, int range){
	int v,max,min;
	spin_lock(&degree_lock);
	v = now; //convert range.
	max = degree + range;
	min = degree - range;
	spin_unlock(&degree_lock);
	if ( v <= max) {
		if( min <= v) return 1; // min-v-max
		else return (v <= max-360);//v-min-max

	}
	else return  (min + 360 <= v);//min-max-v
}


int isLockable(int degree,int range,int target) { //target 0 : read, 1 : write
	int i;
	int flag = 1;
	spin_lock(&degree_lock);
	for(i = degree-range; i <= degree+range ; i++) {
		if(target ==1 && write_locked[convertDegree(i)] != 0) {
			flag= 0;
			break;
		}
		else if(target == 0 && read_locked[convertDegree(i)] != 0) {		
			flag =0;
			break;
		}
	}
	spin_unlock(&degree_lock);
	return flag;
}

int sys_rotlock_read(int degree, int range) {
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	DEFINE_WAIT(wait);
	printk(KERN_DEBUG "rotlock_read\n");
	int i,deg;
	while(!(isInRange(_degree,degree,range) && isLockable(degree, range,1))){

//		printk(KERN_DEBUG "HOLD\n");
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}

//	printk(KERN_DEBUG "AFTER HOLD\n");
	//Increment the number of locks at each degree.
	spin_lock(&locker);

//	printk(KERN_DEBUG "spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotlock_write(int degree, int range) {
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotlock_write\n");
	DEFINE_WAIT(wait);
	int i,deg;
	while(!(isInRange(_degree,degree,range) && isLockable(degree, range,1) && isLockable(degree,range,0))){
//	printk(KERN_DEBUG "W_HOLD\n");
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
//	printk(KERN_DEBUG "W_AFTER HOLD!\n");
	spin_lock(&locker);
//	printk(KERN_DEBUG "W_spin_lock\n");
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]++;
	}
	spin_unlock(&locker);
	return 0;
}

int sys_rotunlock_read(int degree, int range) {
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotunlock_read\n");
	DEFINE_WAIT(wait);
	int i,deg;
	while(!(isInRange(_degree,degree,range))){
		prepare_to_wait(&read_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&read_q,&wait);
	}
	spin_lock_init(&locker); 
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		read_locked[deg]--;
	}
	spin_unlock(&locker);
	return 0;
}


int sys_rotunlock_write(int degree, int range) {
	if(degree <0 || degree >=360 || range <=0 || range>= 180) return -1;
	printk(KERN_DEBUG "rotunlock_write\n");
	DEFINE_WAIT(wait);
	int i,deg;
	while(!(isInRange(_degree,degree,range))){
		prepare_to_wait(&write_q,&wait,TASK_INTERRUPTIBLE);
		schedule();
		finish_wait(&write_q,&wait);
	}
	spin_lock_init(&locker); 
	//Increment the number of locks at each degree.
	spin_lock(&locker);
	for(i = degree-range ; i <= degree+range ; i++) {
		deg = convertDegree(i);
		write_locked[deg]--;
	}
	spin_unlock(&locker);
	return 0;
}
