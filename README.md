# SmartLifeWashingMachine
## abstract
This is the embedded part of a project that aim to change inconvenient parts of using washing machine.

### What are inconvenient parts of using washing machine?
#### 1. Huge costs
Laundromat owner needs lots of moneys to buy machinesIn this case, we want to change the business model to renting machine. But renting machine has some concerns. For example, customer don't know the repaired history. To improve the confidence of laundromat owner renting machine, we combine this project with block chain.

#### 2. Safty
Believe many people have experiences that loosing the clothes when they are using public washing machine. To solve the problem, we give every users password. The washing machine keep locks when anyone is using it. When the washing process is finished, user need to use the password to unlock it. So this can prevent the unsafety sutuation.

#### 3. Inconvience
When we using the washing machine in dormitory, we usually meet the condition that all machines are working. We don't know the accurate left time, so we need to stay there to wait. It is very waste the time. To fix the inconvient, we create a function that users can make reservation by APP and check the left time without going there.

## Demonstration

### Safty
#### User Interface
This is the LCD screen that can show some information when users enter/change the password.

If they want to change the password, they need to enter the old password.
![](https://i.imgur.com/AazfL7T.png)

If the old password correct then enter the new password.
![](https://i.imgur.com/e2WVFSq.png)

After changing password, the password stored on the cloud will change to new password.
> By using MQTT protocol to accomplish this function, the password is stored by AWS device's shadow.

![](https://i.imgur.com/jyo4Szj.png)

#### Warning Message

When the door of washing machine is open by the improper way, the embedded system will send message to the cloud by using MQTT protocol. Then the cloud will send the warning message to the user.
![](https://i.imgur.com/Kef2EmI.png)

### Check Machine's status

The embedded system check machine's status constantly. If the washing machine has any malfunction, embedded system will tell the cloud that the machine is unhealthy. Then cloud will inform maintenance provider to repair it.

![](https://i.imgur.com/h5m5tqh.png)

When the maintenance provider fixed it, they need to press the check-itself button. Then the machine will check by itself. If it really been fixed, it will publish the message to the cloud. The cloud will add the repaired record to the block chain.

![](https://i.imgur.com/LQhXFtd.png)
