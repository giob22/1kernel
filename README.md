# note

## `run.sh`

file che utilizzo per avviare OpenSBI che è l'equivalente dei BIOS.

OpenSBI fornisce SBI per il kernel dei sistemi operativi.

SBI definisce ciò che il firmware fornisce a un sistema operativo.

```bash
#!/bin/bash

set -xue

QEMU=qemu-system-riscv32

$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot
```

- `-machine virt`: avvia una macchina `virt`.
-  `-bios default`: utilizza il firmware predefinito
- `-nographics`: avvia QEMU senza una finestra GUI
- `-serial mon:stdio`: collega input/output standard di QEMU alla porta seriale della macchina virtuale
- `--no-reboot`: se la macchina virtuale si blocca, arresta l'emulatore senza riavviare.



## linker script

Un linker script è un file che definisce il layout di memoria dei file eseguibili. In base a questo layout, il linker assegna indirizzi di memoria a funzioni e variabili.

Punti chiave del linker script nel file `kernel.ld`:

- boot: il punto di ingresso del kernel.
- indirizzo base (`. = 0x80200000`).
- sezione `.text.boot` viene sempre posizionata all'inizio
- l'ordine delle sezioni è importante
- lo stack è sempre l'ultimo

Di cosa sono composte le sezioni:

|sezione|descrizione|
|:--|:--|
|`.text`|contiene il codice del programma|
|`.rodata`|contiene dati costanti di sola lettura|
|`.data`|contiene dati di lettura/scrittura|
|`.bss`|contiene dati di lettura/scrittura con valore iniziale pari a zero| 

Entrando nel merito di ciò che contiene effettivamente il file abbiamo:

`ENTRY(boot)` dichiara che la funzione `boot` è il punto di ingresso del programma.

Il posizionamento di ciascuna sezione viene definito all'interno del blocco `SECTIONS`.

La direttiva `*(.text .text.*)` posiziona la sezione `.text` e tutte le sezioni che iniziano con `.text` da tutti i file in quella posizione.

Il simbolo `.` rappresenta l'indirizzo corrente. Incrementa automaticamente man mano che si vengono inseriti dati, ad esempio con `*(.text)`. L'istruzione `. += 128 * 1024` significa "avanza l'indirizzo corrente di `128KB`". La direttiva `ALIGN(4)` assicura che l'indirizzo corrente venga adattato a un limite di `4 byte`.

Infine, `__bss = .` assegna l'indirizzo corrente al simbolo `__bss`.

## `kernel.c`

L'esecuzione del kernel inizia dalla funzione `boot`, specificata come punto di ingresso nello script del linker. In questa funzione, il puntatore allo stack, `sp`, viene impostato all'indirizzo finale dell'area dello stack definita nel linker script.

Quindi, salta alla funzione `kernel_main`.

NOTA: lo stack cresce verso zero, quindi quando lo utilizziamo lo stacp pointer diminuisce, quindi è importante che sia impostato come l'ultimo indirizzo dell'area dello stack.

La funzione `boot` ha due attributi speciali. L'attributo `__attribute__((naked))` indica al compilatore di non generare codice non necessario prima e dopo il corpo della funzione, come un'istruzione di ritorno. Questo garantisce che il codice assembly inline sia esattamente il corpo della funzione.

Inoltre, è presenta anche l'attributo `__attribute((section(".text.boot")))`, che controlla il posizionamento della funzione nel linker script.

Poiché OpenSBI salta semplicemente a `0x80200000` senza conoscere il punto di ingresso, la funzione `boot` deve essere posizionata a `0x80200000`.

All'inizio del file, ogni simbolo definito nello script del linker viene dichiarato utilizzando `extern char`. In questo caso, siamo interessati solo a ottenere gli indirizzi dei simboli, quindi l'utilizzo del tipo char non è così importante.

Utilizzando `[]` dopo `__bss` ci garantisce che venga restituito il primo indirizzo di quella sezione e non il valore del primo byte di questa.

Con la funzione `memset` andiamo ad inizializzare a `0` la sezione `.bss`.

### Aggiunta in `run.sh`

Le opzioni clang specificate dalla variabile `CFLAGS` sono le seguenti:

- `-std=c11`: utilizza C11
- `-O2`: abilita le ottimizzazioni per generare codice macchina efficiente
- `-g3`: genera la massima quantità di informazioni di debug
- `-Wall`: abilita gli avvisi principali
- `Wextra`: abilita avvisi aggiuntivi
- `--targer=riscv32-unknown-elf`: compilare per RISC-V a 32bit
- `--ffreestanding`: non utilizzare librerie standar dell'ambiente host. Perché il codice viene eseguito su hardware nudo (virtualizzato).
- `-fuse-ls=lld`: Utilizzare il linker LLVM (`ls.lld`)
- `-fno-stack-protector`: disattiva la protezione dello stack non necessaria per evitare comportamente imprevisti nella manipolazione dello stack
- `-nostdlinb`: non collegare la libreria standard
- `-Wl,-Tkernel.ld`: specifica il linker script
- `-Wl, -Map=kernel.map`: genera un file mappa, risultato dell'allocazione del linker

`-Wl`, significa passare le informazioni all linker invece che al compilatore C. Il comando `clang` esegue la compilazione C ed esegue internamente il linker.

## capitolo 5, Hello world

SBI è una "API per il sistema operativo". Per chiamare SBI e utilizzare la sua funzione, utilizziamo l'istruzione `ecall`.

.



