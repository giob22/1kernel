# VIRTIO

VIRTIO è uno standard aperto per I/O nei dispositivi virtuali. È lo strato che permette a una macchina virtuale (Guest) di comunicare in modo efficiente con l'hypervisor (Host), come ad esempio KVM, QEMU o VirtualBox.

## virtqueues

Il meccanismo per il trasporto di dati in massa sui dispositivi virtio è chiamato virtqueue. Ogni dispositivo può avere zero o più virtqueue.

Un dispositivo virtio può avere un massimo di 65536 virtqueue. Ogni virtqueue è identificata da un indice virtqueue. Un indice virtqueue ha un valore compreso tra $[0, 65535]$.