#include <stdio.h>
#include <usb.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <unistd.h>

#define IN 0x83
#define OUT 0x03

#define VID 0x18D1
#define PID 0x4E12

#define ACCESSORY_PID 0x2D01
#define ACCESSORY_PID_ALT 0x2D00

#define BUFFER 1024

static int mainPhase();
static int isUsbAccessory (void);
static int init(void);
static int deInit(void);
static void error(int code);
static int setupAccessory(
  const char* manufacturer,
  const char* modelName,
  const char* description,
  const char* version,
  const char* uri,
  const char* serialNumber);

//static
static struct libusb_device_handle* handle;
static char stop;
static char success = 0;

int main (int argc, char *argv[]){
  if (isUsbAccessory() < 0) {
    if(init() < 0)
      return;
    if(setupAccessory(
                      "Poly's Factory",
                      "Android Oepn Accessory Demo",
                      "Android Oepn Accessory Demo",
                      "1.0",
                      "http://d.hatena.ne.jp/thorikawa/",
                      "0000000012345678") < 0) {
      fprintf(stdout, "Error setting up accessory\n");
      deInit();
      return -1;
    }
  }
  if(mainPhase() < 0){
    fprintf(stdout, "Error during main phase\n");
    deInit();
    return -1;
  }  
  deInit();
  fprintf(stdout, "Done, no errors\n");
  return 0;
}

static int mainPhase(){
  unsigned char buffer[BUFFER];
  int response = 0;
  static int transferred;

  int i;
  int index=1;
  fprintf(stdout, "start main Phase\n");
  while (libusb_bulk_transfer(handle, IN, buffer, BUFFER, &transferred, 0) == 0) {
    int offset;
    // 8文字単位で読み込む
    for(offset = 0; offset < transferred; offset += 8){
      // 最初の4文字(端末の傾き)を読み込む
      char tmp = buffer[offset+4];
      buffer[offset+4] = '\0';
      FILE *fp;
      fp = fopen("/workspace/box2d-with-usb/angle.txt", "w");
      if (fp != NULL) {
        buffer[transferred] = '\0';
        fprintf(stdout, "%s\n", buffer+offset);
        fprintf(fp, "%s", buffer+offset);
        fclose(fp);
      } else {
        fprintf(stderr, "file error1\n");
      }
      buffer[4] = tmp;
      
      // 続く4文字(ボールが追加されたかどうか)を読み込む
      tmp = buffer[offset+8];
      buffer[offset+8] = '\0';
      char flag[4];
      int addBall = atoi(buffer+offset+4);
      if(addBall){
        FILE *fp2;
        fp2 = fopen("/workspace/box2d-with-usb/ball.txt", "w");
        if (fp2 != NULL) {
          fprintf(stdout, "add ball\n");
          fprintf(fp2, "%d", index);
          fclose(fp2);
        } else {
          fprintf(stderr, "file error2\n");
        }
        index++;
      }
    }
  }
  error(response);
  return -1;
}

static int isUsbAccessory () {
  // 端末がすでにUSB Accessory Modeかどうかを判定する
  int res;
  libusb_init(NULL);
  if((handle = libusb_open_device_with_vid_pid(NULL, VID,  ACCESSORY_PID)) == NULL) {
    fprintf(stdout, "Device is not USB Accessory Mode\n");
    res = -1;
  } else {
    // already usb accessory mode
    fprintf(stdout, "Device is already USB Accessory Mode\n");
    libusb_claim_interface(handle, 0);
    res = 0;
  }
  return res;
}

static int init(){
  if((handle = libusb_open_device_with_vid_pid(NULL, VID, PID)) == NULL){
    fprintf(stdout, "Problem acquireing handle\n");
    return -1;
  }
  libusb_claim_interface(handle, 0);
  return 0;
}

static int deInit(){
  if(handle != NULL)
    libusb_release_interface (handle, 0);
  libusb_exit(NULL);
  return 0;
}

static int setupAccessory(
  const char* manufacturer,
  const char* modelName,
  const char* description,
  const char* version,
  const char* uri,
  const char* serialNumber){

  unsigned char ioBuffer[2];
  int devVersion;
  int response;
  int tries = 5;

  // DeviceがAndroid accessory protocolをサポートしているか判定する
  response = libusb_control_transfer(
    handle, //handle
    0xC0, //bmRequestType
    51, //bRequest
    0, //wValue
    0, //wIndex
    ioBuffer, //data
    2, //wLength
        0 //timeout
  );

  if(response < 0){error(response);return-1;}

  devVersion = ioBuffer[1] << 8 | ioBuffer[0];
  fprintf(stdout,"Verion Code Device: %d\n", devVersion);
  
  usleep(1000);//sometimes hangs on the next transfer :(

  // Accessory Identificationを送信する
  response = libusb_control_transfer(handle,0x40,52,0,0,(char*)manufacturer,strlen(manufacturer),0);
  if(response < 0){error(response);return -1;}
  response = libusb_control_transfer(handle,0x40,52,0,1,(char*)modelName,strlen(modelName)+1,0);
  if(response < 0){error(response);return -1;}
  response = libusb_control_transfer(handle,0x40,52,0,2,(char*)description,strlen(description)+1,0);
  if(response < 0){error(response);return -1;}
  response = libusb_control_transfer(handle,0x40,52,0,3,(char*)version,strlen(version)+1,0);
  if(response < 0){error(response);return -1;}
  response = libusb_control_transfer(handle,0x40,52,0,4,(char*)uri,strlen(uri)+1,0);
  if(response < 0){error(response);return -1;}
  response = libusb_control_transfer(handle,0x40,52,0,5,(char*)serialNumber,strlen(serialNumber)+1,0);
  if(response < 0){error(response);return -1;}

  fprintf(stdout,"Accessory Identification sent\n", devVersion);

  // DeviceをAccessory modeにする
  response = libusb_control_transfer(handle,0x40,53,0,0,NULL,0,0);
  if(response < 0){error(response);return -1;}

  fprintf(stdout,"Attempted to put device into accessory mode\n", devVersion);

  if(handle != NULL){
    libusb_close(handle);
  }

  fprintf(stdout, "connect to new PID...\n");
  for(;;){ //attempt to connect to new PID, if that doesn't work try ACCESSORY_PID_ALT
    tries--;
    if((handle = libusb_open_device_with_vid_pid(NULL, VID, ACCESSORY_PID)) == NULL){
      if(tries < 0){
        return -1;
      }
    }else{
      break;
    }
    sleep(1);
  }
  
  // Interface #0をhandleに紐づける
  fprintf(stdout, "claim usb accessory I/O interface\n");
  response = libusb_claim_interface(handle, 0);
  if(response < 0){error(response);return -1;}

  fprintf(stdout, "Interface claimed, ready to transfer data\n");
  return 0;
}

static void error(int code){
  fprintf(stdout,"\n");
  switch(code){
  case LIBUSB_ERROR_IO:
    fprintf(stdout,"Error: LIBUSB_ERROR_IO\nInput/output error.\n");
    break;
  case LIBUSB_ERROR_INVALID_PARAM:
    fprintf(stdout,"Error: LIBUSB_ERROR_INVALID_PARAM\nInvalid parameter.\n");
    break;
  case LIBUSB_ERROR_ACCESS:
    fprintf(stdout,"Error: LIBUSB_ERROR_ACCESS\nAccess denied (insufficient permissions).\n");
    break;
  case LIBUSB_ERROR_NO_DEVICE:
    fprintf(stdout,"Error: LIBUSB_ERROR_NO_DEVICE\nNo such device (it may have been disconnected).\n");
    break;
  case LIBUSB_ERROR_NOT_FOUND:
    fprintf(stdout,"Error: LIBUSB_ERROR_NOT_FOUND\nEntity not found.\n");
    break;
  case LIBUSB_ERROR_BUSY:
    fprintf(stdout,"Error: LIBUSB_ERROR_BUSY\nResource busy.\n");
    break;
  case LIBUSB_ERROR_TIMEOUT:
    fprintf(stdout,"Error: LIBUSB_ERROR_TIMEOUT\nOperation timed out.\n");
    break;
  case LIBUSB_ERROR_OVERFLOW:
    fprintf(stdout,"Error: LIBUSB_ERROR_OVERFLOW\nOverflow.\n");
    break;
  case LIBUSB_ERROR_PIPE:
    fprintf(stdout,"Error: LIBUSB_ERROR_PIPE\nPipe error.\n");
    break;
  case LIBUSB_ERROR_INTERRUPTED:
    fprintf(stdout,"Error:LIBUSB_ERROR_INTERRUPTED\nSystem call interrupted (perhaps due to signal).\n");
    break;
  case LIBUSB_ERROR_NO_MEM:
    fprintf(stdout,"Error: LIBUSB_ERROR_NO_MEM\nInsufficient memory.\n");
    break;
  case LIBUSB_ERROR_NOT_SUPPORTED:
    fprintf(stdout,"Error: LIBUSB_ERROR_NOT_SUPPORTED\nOperation not supported or unimplemented on this platform.\n");
    break;
  case LIBUSB_ERROR_OTHER:
    fprintf(stdout,"Error: LIBUSB_ERROR_OTHER\nOther error.\n");
    break;
  default:
    fprintf(stdout, "Error: unkown error\n");
  }
}
