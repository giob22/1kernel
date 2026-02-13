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

### firmware

Il firmware è un tipo particolare di software che è "scritto nel ferro". Mentre il kernel può essere aggiornato o cambiato facilmente, il firmware è solitamente fornito dal produttore della scheda madre o dalla CPU.

Il suo compito è quello di:

- Inizializzare l'hardware: quando si accende la CPU, questa non sa nemmeno di avere la RAM. Il firmware è il primo codice che gira e configura i vari componenti.
- Preparare il campo per il OS: una volta configurato l'hardware, il firmware cerca il kernel, lo carica in memoria e lo avvia.
- Fornire servizi critici: anche dopo l'avvio, rimane residente in memoria per gestire compiti che il kernel non può o non deve gestire.

Quindi nei sistemi moderni la distinzioni tra le modalità di esecuzione non è più quella tra kernel mode e user mode, ma ci sono tre livelli:

- User mode
- Kernel mode
- Machine mode

---

OpenSBI è quindi il firmware e attraverso la sua interfaccia, SBI, il kernel può chiedere di gestire delle funzionalità.

La funzione `sbi_call` cosa fa tecnicamente:

- registri specifici: il protocollo SBI stabilisce che i parametri devono stare nei registri da `a0` a `a5`, l'ID della funzione in `a6` e l'ID dell'estensione in `a7`.
- l'istruzione `ecall`: è l'istruzione fondamentale. Quando la CPU incontra `ecall`, mette in pausa il kernel e salta all'indirizzo dell'OpenSBI.
- il ritorno: dopo aver eseguito il compito, l'OpenSBI mette il risultato nei registri `a0` (errore) e `a1` (valore) e restituisce il controllo al kernel.

### differenza tra Estensione EID e Funzione FID

L'interfaccia SBI è modulare. Non è un unico blocco di funzioni, ma una collezioni di "pacchetti" (estensioni).

- EID: identifica la categoria della chiamata. Dice a OpenSBI a quale modulo appartiene la richiesta.
- FID: identifica la sottofunzione specifica all'interno del modulo specificato da EID.

---

Con le istruzioni `register long a0 __asm__("a0") = arg0;` stiamo dando ordini precisi al compilatore su come usare l'hardware della CPU.

In C standard, di solito non ci interessa dove il compilatore salva una variabile. Ma quando dobbiamo fare una SBI call, il protocollo è rigido: i dati devono trovarsi in registri specifici prima di lanciare l'istruzione `ecall`.

La sintassi `register long a0 __asm__("a0") = arg0` di divide in tre parti:

- `register`: è il suggerimento al compilatore di inserire questa variabile nei registri e non nello stack.
- `__asm__("a0")`: questo è il comando cruciale. Dice al compilatore di non scegliere un registro a caso ma deve usare il registro fisico `a0`.
- `= arg0` copia il valore del parametro della funzione dentro quel registro.

Nella chiamata `ecall` ci sono inoltre altre informazioni che dobbiamo dare al compilatore riguardanti i registri della CPU.

La struttura segue tale schema: `__asm__ ("codice" : output : input : clobber);`

1) output operands: `"=r"(a0), "=r"(a1)`
   
   Questa prima parte dice al compilatore che l'istruzione `ecall`, in questo caso, termina i valori di output sono inseriti all'interno dei registri specificati.

   - `=` indica che il registro viene scritto, è un output
   - `r` indica che deve usare un registro
2) input operands: `"r"(a0), "r"(a1), ..., "r"(a7)`
   
   Qui invece stiamo dicendo al compilatore quali variabili devono essere pronte prima di eseguire `ecall`.

   Quindi stiamo evitando che i registri necessari all'esecuzione della `ecall` siano modificati.
3) clobber list: `memory`
   
   Questo è un avvertimento fondamentale per l'ottimizzazione del compilatore.

   - `memory` dice al compilatore che la funzione potrebbe utilizzare la memoria RAM, quindi sporcare il contenuto dello stack.

## `common.h`

Il contenuto di `common.h` riguarda delle definizioni e dei prototipi di funzione da esporre alle applicazioni utente che fanno uso di tale libreria.

La parte più importante riguarda le definizioni, infatti sono presenti:

```c
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
```

Nella costruzione di un kernel non possiamo utilizzare libreria standard del C (`stdio.h`), quindi dobbiamo definire un modo per gestire i parametri extra nel caso di funzioni **variadiche**, ovvero funzioni che accettano una lista variabile di argomenti.

Invece di scrivere codice assembly complesso per estrarre i parametri dallo stack, si usano delle scorciatoie fornite dal compilatore.

|Macro|Cosa fa|
|:---|:---|
|`va_list`|Il cursore. Serve per creare la variabile che farà da puntatore o cursore corrente.|
|`va_start`|Il punto di partenza. Inizializza il cursore: `va_start(cursore, ultimo_argomento_fisso)`.|
|`va_arg`|Leggi e avanza. Cuore del meccanismo, legge il valore alla posizione attuale del cursore interpretandolo rispetto al tipo specificato.|
|`va_end`|Segnala che abbiamo finito di leggere gli argomenti. Spesso questa macro non fa nulla o imposta il puntatore a NULL.|

Tutte le macro puntano a qualcosa che inizia con `__builtin_`. Questo accade perché la gestione degli argomenti variabili dipende totalmente dall'architettura della CPU. In RISC-V:

- i primi 8 argomenti vanno nei registri, gli altri vanno nello stack
- il compilatore sa esattamente dove sono finiti. Usando `__builtin_`, chiediamo al compilatore di generare il codice specifico per andare a recuperare quei valori nel posto giusto senza dover necessariamente conoscere i dettagli dell'assembly.

Queste macro sono necessarie per poter effettivamente stampare una formated string.

### Nella funzione `printf()`

Nel caso di `%d` dobbiamo far attenzione quando `value` è un numero negativo.

Il motivo del pericolo sta nel fatto che l'intervallo di rappresentazione dei numeri interi non è simmetrico, ciò significa che la quantità di numeri negativi è maggiore rispetto i positivi se escludiamo lo zero dal conteggio.

Quindi quando voglio stampare il numero negativo più piccolo possibile, non posso stampare il `-` e successivamente lavorare con lo stesso numero ma cambiato di segno.


L'intervallo di rappresentazione per un intero su `4byte`:

$$[-2.147.483.648; -2.147.483.647]$$

Significa che se `value = -2.147.483.648`, allora non posso fare `value = -value`, altrimenti andremo in *overflow*.

Utilizzando invece `unsigned magnitude` non ho problemi per rappresentare tale numero. Ovviamente le variabili `unsigned` non rappresentano i numeri in complemento a 2, come avviene per i numeri interi.

Questo significa che se faccio `unsigned magnitude = -1`:

- `-1` in complemento a 2 equivale a `11111111111111111111111111111111`
- `magnitude`, mantiene valori rappresentati in binario; il corrispondente valore a quella sequenza di bit sarà: `4.294.967.295`

Una volta ottenuto questo risultato su `magnitude`, posso nuovamente moltiplicare per `-1` il risultato.

Eseguendo la moltiplicazione in complemento a 2 ottengo: `(-4.294.967.295)`$_{10}$ = `(00000000000000000000000000000001)`$_{2}$.

Tale numero nella rappresentazione binaria è pari a `1`. Quindi ottengo il valore che stavo cercando.

Questo metodo quindi è comodo per evitare *overflow* dovuto al fatto che l'intervallo di rappresentazione non è simmetrico



## capitolo 6

Essendo che non possiamo utilizzare funzioni della libreria C standard, implementiamo da 0 alcune delle più utili.

- `paddr_t`: un tipo che rappresenta gli indirizzi di memoria fisici.
- `vaddr_t`: un tipo che rappresenta gli indirizzi di memoria virtuale. Equivalente a `uintptr_t` nella libreria standard.
- `align_up`: arrotonda `value` al multiplo più vicino di `align`. `align` deve essere una potenza di 2.
- `is_aligned`: varifica se `value` è un multiplo di `align`. `align` deve essere una potenza di 2.
- `offsetof`: restituisce l'offset di un membro all'interno di una struttura, ovvero il numero di byte dall'inizio della struttura in cui è posizionato il membro.

`align_up` e `is_aligned` sono utili quando si gestisce l'allineamento della memoria. Ad esempio, `align_up(0x1234, 0x1000)` restituisce `0x2000`.

Inoltre, `is_aligned(0x2000,0x1000)` restituisce `true`, ma `is_aligned(0x2f00, 0x1000)` restituisce `false`.



## kernel panic

Se definissimo il kernel panic come una funzione `__LINE__` e `__FILE__` mostrerebbero il nome del file e il numero di riga in cui è definita la funziona chiamata.

Invece definendola come macro questa ci fornisce effettivamente informazioni sulla posizione dell'errore che ha mandato in panico il kernel.

La macro consta di due costrutti C che sono il `do while` e il `while`.

Il primo ciclo `while(0)`, viene eseguito una sola volta. Questo è un modo comune per definire macro composte da più istruzioni.

Racchiudere semplicemente tra `{ ... }` può portare a comportamenti indesiderati se combinato con istruzioni come `if`.

Inoltre, si noti la `\` alla fine di ogni riga. Sebbene la macro sia definita su più righe, i caratteri di nuova riga vengono ignorati quando espansa.

Il secondo idioma `#__VA_ARGS__`. Si tratta di un'utile estensione del compilatore per definire macro che accettano un numero variabile di argomenti. `##` rimuove il precedente `,` quando gli argomenti variabili sono vuoti.

Questo consente la compilazione anche quando è presente un solo argomento, come `PANIC("booted!")`.

## capitolo 8 (Exception)

L'eccezione è una funzionalità della CPU che consente al kernel di gestire vari eventi, come accessi alla memoria non validi (page fault), istruzioni illegali e chiamate di sistema.

L'eccezione è simile ad un meccanismo `try-catch` assistito dall'hardware in c++ o java.

Finché la CPU non incontra la situazione in cui è richiesto l'intervento del kernel, continua ad eseguire il programma.

La differenza fondamentale rispetto `try-catch` è che il kernel può riprendere l'esecuzione dal punto in cui si è verificata l'eccezione, come se nulla fosse accaduto.

Le eccezioni possono essere attivate anche in kernel mode e nella maggior parte dei casi si tratta di bug fatali del kernel.

Se QEMU si resetta inaspettatamente o il kernel non funziona come previsto, è probabile che si sia verificata un'eccezione.

È consigliato implementare un gestore delle eccezioni in anticipo per evitare crash in modo corretto in caso di kernel panic.

### vita di una eccezione

In RISC-V, un eccezione verrà gestita come segue:

1) la CPU controlla il registro `medeleg` per determinare quale modalità operativa debba gestire l'eccezione.
  
  Utilizzando OpenSBI, questo è già configurato per gestire le eccezioni U-mode e S-mode nel gestore S-mode.

2) La CPU salva il suo stato in vari **CSR**
3) Il valore del registro `stvec` viene impostato sul *program* *counter*, saltando al gestore delle eccezioni del kernel.
4) Il gestore delle eccezioni salva i registri di uso generale e gestisce l'eccezione.
5) Una volta completata l'operazione, il gestore delle eccezioni ripristina lo stato di esecuzione salvato e richiama l'istruzione `sret` per riprendere l'esecuzione dal punto in cui si è verificata l'eccezione.

Le richieste di risposta al client (CSR) aggiornate nel passaggio 2 sono principalmente le seguenti.

L'eccezione del kernel determina le azioni necessarie in base alle richieste di risposta al client (CSR).

---

CSR: *control status register* sono fondamentali perché rappresentano il "pannello di controllo" della CPU, in particolare per l'architettura **RISC-V**.

**A cosa servono**:

I CSR permettono al sistema operativo di interagire con l'hardware per compiti che vanno oltre alla semplice matematica. Come ad esempio:

- gestione delle eccezioni e interrupt: sapere perché il programma è crashato o gestire un timer.
- privilegi: passare dalla modalità utente alla modalità kernel.
- memoria virtuale: configurare dove si trova la tabella delle pagine.
- informazioni di sistema: leggere l'ora esatta o il numero di cicli di clock eseguiti.

In questo caso vengono utilizzati per le eccezioni.

I più importanti sono:

- `stvec` (supervisor trap vector base address): contiene l'indirizzo della funzione che il kernel deve eseguire quando si verifica un'eccezione.
- `scause` (supervisor cause): indica il motivo dell'eccezione.
- `sepc` (supervisor exception program counter) salva l'indirizzo dell'istruzione che era in esecuzione quando è avvenuta l'eccezione, così il kernel sa dove tornare una volta finito il lavoro.
- `sscratch`: registro "jolly" usato per salvare temporaneamente un valore.
- `stval`: informazioni aggiuntive sull'eccezione.
- `sstatus`: modalità operativa quando l'eccezione si è verificata.

---

Il punto di ingresso del gestore dell'eccezione sarà inserito all'intero del registro `stvec`.

Utilizziamo la macro `__attribute__((aligned(4)))` per far in modo che l'indirizzo associato alla entry point sia un multiplo di 4.

A cosa serve?

- La CPU RISC-V legge le istruzioni a blocchi. Se il processore cerca di caricare un'istruzione a 32bit da un indirizzo che non è divisibile per 4, la CPU solleverà un eccezione chiamata **Instruction Address Misaligned**.

Nel punto di ingresso viene chiamata la seguente funzione: `handle_trap`. Per gestire l'eccezione in linguaggio C.

---

Spiegazione di ciò che accade in `kernel_entry`:

- Salvataggio del contesto:
  
  quando entriamo qui, i registri della CPU contengono i valori del programma che è stato interrotto. Dobbiamo salvarli in modo che il programma non capisca che effettivamente è stato eseguito il kernel.

  Quindi il programma non sà che i propri registri potrebbero esser stati sporcati.

  ```asm
  csrw sscratch, sp
  addi sp, sp, -4 * 31
  ```

  - `csrw` salva il valore attuale dello stack pointer (`sp`) nel registro speciale `sscratch`.

   Il motivo sta nel fatto che modificheremo lo stack, quindi non vogliamo perdere il punto originale.

  - `addi` crea sullo stack dello spazio dove verrà salvato tutto.
  
  ```asm
  sw ra,  4 * 0(sp)
  sw gp,  4 * 1(sp)
  ...
  sw s11, 4 * 29(sp)
  ```

  - `sw` (store word): copia il contenuto di ogni registro nello spazio che abbiamo creato sullo stack. 
  
  infine abbiamo:

  ```asm
  csrr a0, sscratch
  sw a0, 4 * 30(sp)
  ```

  Salviamo il vecchio `sp`, che avevamo inserito in `sscratch` nell'ultimo spazio libero sullo stack.

  Abbiamo infine tutti i registri salvati in memoria.
- Chiamata al gestore in C
  
  ```asm
  mv a0, sp
  call handle_trap
  ```
  - `mv` copia l'indirizzo attuale dello stack nel registro `a0`. In C, `a0` è il primo argomento di una funzione.
  - `call handle_trap` salta alla funzione C `handle_trap`. Qui il kernel analizzerà l'accaduto e deciderà cosa fare.
- Ripristino del contesto
  
  Quando `handle_trap` termina, dobbiamo rimettere tutto a posto come se nulla fosse successo.

  ```asm
  lw ra,  4 * 0(sp)
  ...
  lw s11, 4 * 29(sp)
  lw sp,  4 * 30(sp)
  ```

  - `lw` (load word) prende il valore dalla memoria (stack) e li rimette nei registri della CPU.
  - L'ultima istruzione è cruciale, ripristina lo stack pointer originale, eliminando di fatto lo spazio cha avevamo creato per memorizzare i registri.
- Ritorno final
  
  ```asm
  sret
  ```

  - `sret` (supervisor return) non è una normale `ret`, infatti:
  
  1) Torna all'indirizzo salvato nel registro `sepc` (prima dell'interruzione del programma)
  2) Ripristina la modalità di privilegio precedente
  3) Riabilita gli interrupt


Dobbiamo definire le macro e le strutture che stiamo utilizzando in `kernel.h`.

In particolare:

```c
#define  READ_CSR(reg) \
	({ \
		unsigned long __tmp; \
		__asm__ __volatile__ ("csrw " #reg ", %0" :: "r"(__tmp)); \
		__tmp; \
	})
```

è la macro che mi permette di leggere il valore di CSR e di restituirlo al codice C.

È una **Statement Expression** (`({ ... })`), ovvero un estensione di GCC che permette di mettere un blocco di codice dentro un'espressione. L'ultima riga `__tmp;` diventa il valore di ritorno del blocco.

--- 

La `trap_frame` rappresenta lo stato del programma salvato in `kernel_entry`.

L'ultima cosa che dobbiamo fare è dire alla CPU dove si trova il gestore delle eccezioni. Si fa impostando il registro `stvec` nella funzione `kernel_main`.

## memory allocation

Definiamo un semplice allocatore di memoria. Prima di allocare memoria è necessario definire la regione di memoria che l'allocatore deve gestire.

```linker
. = ALIGN(4096);
__free_ram = .;
. += 64 * 1024 * 1024; /* 64MB */
__free_ram_end = .;
```

Questo aggiunge nuovi simboli: `__free_ram` e `__free_ram_end`.

Stiamo definendo un'area di memoria dopo lo spazio dello stack. La dimensione dello spazio è un valore arbitrario e `. = ALIGN(4096)` garantisce che sia allineato a un confine di `4KB`.

I sistemi operativi su `x86-64` determinano le regioni di memoria disponibili ottenendo informazioni dall'hardware al momento dell'avvio (ad esempio, `GetMemoryMap` di UEFI).

### algoritmo di allocazione

Invece di allocare in byte, come il `malloc`, alloca un'unità più grande chiamata "pagina". 1 pagina è tipicamente `4KB`.

La funzione `alloc_pages` alloca dinamicamente `n` pagine di memoria e restituisce l'indirizzo di partenza.

- `netx_paddr` è definito come una variabile statica. Ciò significa che, a differenza delle variabili locali, il suo valore viene mantenuto tra una chiamata di funzione e la successiva.
  
  Indica l'indirizzo di inizio della prossima area da assegnare. Quando si assegna, `next_paddr` viene avanzata dalla dimensione assegnata.

  Inizialmente tale variabile ha l'indirizzo `__free_ram`. Questo significa che la memoria viene assegnata in sequeza a partire da `__free_ram`.

  Essendo che la `__free_ram` è allineata su un confine di `4KB`, come abbiamo definito nel linker script, la funzione `alloc_pages` restituisce sempre un indirizzo allienato.

  Se cerca di allocare oltre `__free_ram_end` si verifica un kernel panic.

- `memset` garantisce che l'area di memoria allocata sia sempre riempita da zeri. Questo serve ad evitare problemi difficili da debug causati da memoria non inizializzata.

Tutta via con questo algoritmo non abbiamo la possibilità di liberare la memoria allocata. Quindi dovremmo implementare altri algoritmi che ci permettono di gestire ciò, come il buddy system.


## process

Un processo è un'istanza di un'applicazione. Ogni processo ha un proprio contesto di esecuzione e risorse indipendenti, come uno spazio di indirizzamento virtuale.

Molti sistemi operativi forniscono il contesto di esecuzione come un concetto separato, chiamto *thread*. Per semplicità utilizziamo processi che hanno un unico thread, ovvero un unico contesto di esecuzione.

Definiamo il Process Control Block (PCB).

```c
#define PROCS_MAX 8       // Maximum number of processes

#define PROC_UNUSED   0   // Unused process control structure
#define PROC_RUNNABLE 1   // Runnable process

struct process {
    int pid;             // Process ID
    int state;           // Process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp;          // Stack pointer
    uint8_t stack[8192]; // Kernel stack
}
```

Lo stack del kernel contiene registri CPU salvati, indirizzi di ritorno e variabili globali. Preparando uno stack kernel per ogni processo, possiamo implementare il context switching salvando e ripristinando i registri della CPU e cambiando il puntatore dello stack.

> Esiste un altro approccio chiamato "single kernel stack". Invece di avere uno stack kernel per ogni processo, c'è un solo stack per CPU.
>
> Cerca *stackless async* se interessa.

La funzione `switch_context` implementa il cambio di contesto.

Infatti questa salva i registri del chiamante sullo stack, cambia il puntatore dello stack e poi carica i registri presenti sul nuovo stack.

---

Motivo per cui abbiamo salvato solo 13 registri dei 31 che abbiamo salvato durante il `kernel_entry`.

- Regola dell'ABI (Application Binary Interface)
   
   In RISC-V, i registri sono divisi in due categorie:

   - Caller-Saved (Temporanei): `t0`-`t6`, `a0`-`a7`.
     - Chi chiama una funzione, se vuole preservare questi registri, deve salvarli prima di fare la chiamata. La funzione chiamata, quindi, può sovrascriverli liberamente.

        Il salvataggio di tali registri è gestito dal compilatore.
   - Callee-Saved (Preservati): `s0`-`s11`, `sp`
     - Il chiamato se vuole utilizzare tali registri allora è costretto a salvarli e ripristinarli alla fine. Chi chiama la funzione si aspetta di ritrovarli intatti.

---

Implementiamo una funzione di inizializzazione del processo: `create_process`. Prende il punto di ingresso come parametro e restituisce un puntatore allo `struct process` creato.

### `create_process`

Prepara un nuovo processo per eseguire

La parte in cui operiamo sullo stack consiste nel creare un contesto fittizio per ingannare la funzione `switch_context`.

Infatti cosa fa la funzione `switch_context`:

1) salva i registri del vecchio processo
2) scambia lo stack pointer
3) carica 13 valori dallo stack del nuovo processo nei registri
4) esegue `ret`

Il problema è che un nuovo processo, che non ha mai eseguito prima, non ha registri salvati. Per questo per evitare che nei registri finisca spazzatura, quello che si trova nello stack, o memoria non valida, si realizza un contesto fittizio.

Vediamo cosa accade:

- impostiamo `sp` alla fine dell'array dello stack, ovvero il punto più in alto in memoria.
- riempie lo stack con 12 zeri
- infine la riga più importante: `*--sp = (uint8_t) pc; //ra`
  
  Stiamo mettendo il valore `pc` nella posizione dove `switch_context` si aspetta di trovare il **Return Address** (`ra`).

Una volta fatto ciò si riempiono i campi contenuti all'interno della struttura `struct process` che identificano il singolo processo e il suo contesto di esecuzione.

### TEST

implementiamo due processi che eseguiranno simultaneamente

```c
void delay(void){
	for (int i = 0; i < 30000000; i++){
		__asm__ __volatile__("nop"); // non fare nulla
	}
}

struct process *proc_a;
struct process *proc_b;


void proc_a_entry(void){
	printf("starting process A\n");
	while (1) {
		putchar('A');
		switch_context(&proc_a->sp, &proc_b->sp);
		delay();
	}
}
void proc_b_entry(void){
	printf("starting process B\n");
	while (1) {
		putchar('B');
		switch_context(&proc_b->sp, &proc_a->sp);
		delay();
	}
}
```

Questi saranno i due processi che intercalano la loro esecuzione sul processore.

Nel `kernel_main` dobbiamo quindi creare questi due processi:

```c
void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    proc_a = create_process((uint32_t) proc_a_entry);
    proc_b = create_process((uint32_t) proc_b_entry);
    proc_a_entry();

    PANIC("unreachable here!");
}
```

il risultato dell'esecuzione sarà:

```bash
make run

starting process A
Astarting process B
BABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAQE
```

## scheduler

Non possiamo eseguire i vari processi come abbiamo fatto prima, in cui erano i processi stessi a passare l'esecuzione ai successivi.

Deve esserci una funzionalità del kernel che permette di eseguire questo servizio in modo trasparente dal punto di vista dei processi.

Ovvero c'è la necessità di implementare uno *scheduler*, un programma del kernel che decide il processo successivo.

Implementiamo la funzione di `yield`.

`yield` viene spesso utilizzata come nome per un'API che consente di cedere volontariamente la CPU a un altro processo.

Abbiamo introdotto due variabili globali: `current_proc` che indica il processo attualmente in corso, `idle_proc` che si riferisce al processo idle, che è "il processo da eseguire quando non ci sono processi eseguibili".

L' `idle_proc` viene creato all'avvio come processo con PID pari a `0`.

Dentro `kernel_main` è presente la sua creazione, è un processo di sistema.

>La funzione `yield` consiste in rilascio volontario della CPU di un processo a favore degli altri pronti ad eseguire.

Possiamo notare che l'algoritmo di scheduling si basa su una politica FCFS.

### modifiche nel gestore delle eccezioni

Nel gestore delle eccezioni, salva lo stato di esecuzione sullo stack. Tuttavia, dato che ora usiamo kernel stack separati per ogni processo, dobbiamo aggiornarlo leggermente.

Qui la vecchia versione: 

```c
__attribute__((naked)) // per evitare che il compilatore modifichi le istruzioni per ottimizzarle
__attribute__((aligned(4))) // per assicurarsi che l'entry point della funzione sia un multiplo di 4
void kernel_entry(void){
	__asm__ __volatile__(
		"csrw sscratch, sp\n"
		"addi sp, sp, -4 * 31\n"
		
		"sw ra,  4 * 0(sp)\n"
		"sw gp,  4 * 1(sp)\n"
		"sw tp,  4 * 2(sp)\n"
		"sw t0,  4 * 3(sp)\n"
		"sw t1,  4 * 4(sp)\n"
		"sw t2,  4 * 5(sp)\n"
		"sw t3,  4 * 6(sp)\n"
		"sw t4,  4 * 7(sp)\n"
		"sw t5,  4 * 8(sp)\n"
		"sw t6,  4 * 9(sp)\n"
		"sw a0,  4 * 10(sp)\n"
		"sw a1,  4 * 11(sp)\n"
		"sw a2,  4 * 12(sp)\n"
		"sw a3,  4 * 13(sp)\n"
		"sw a4,  4 * 14(sp)\n"
		"sw a5,  4 * 15(sp)\n"
		"sw a6,  4 * 16(sp)\n"
		"sw a7,  4 * 17(sp)\n"
		"sw s0,  4 * 18(sp)\n"
		"sw s1,  4 * 19(sp)\n"
		"sw s2,  4 * 20(sp)\n"
		"sw s3,  4 * 21(sp)\n"
		"sw s4,  4 * 22(sp)\n"
		"sw s5,  4 * 23(sp)\n"
		"sw s6,  4 * 24(sp)\n"
		"sw s7,  4 * 25(sp)\n"
		"sw s8,  4 * 26(sp)\n"
		"sw s9,  4 * 27(sp)\n"
		"sw s10, 4 * 28(sp)\n"
		"sw s11, 4 * 29(sp)\n"
		
		"csrr a0, sscratch\n"
		"sw a0, 4 * 30(sp)\n"
		
		
		"mv a0, sp\n"
		"call handle_trap\n"
		
		"lw ra,  4 * 0(sp)\n"
		"lw gp,  4 * 1(sp)\n"
		"lw tp,  4 * 2(sp)\n"
		"lw t0,  4 * 3(sp)\n"
		"lw t1,  4 * 4(sp)\n"
		"lw t2,  4 * 5(sp)\n"
		"lw t3,  4 * 6(sp)\n"
		"lw t4,  4 * 7(sp)\n"
		"lw t5,  4 * 8(sp)\n"
		"lw t6,  4 * 9(sp)\n"
		"lw a0,  4 * 10(sp)\n"
		"lw a1,  4 * 11(sp)\n"
		"lw a2,  4 * 12(sp)\n"
		"lw a3,  4 * 13(sp)\n"
		"lw a4,  4 * 14(sp)\n"
		"lw a5,  4 * 15(sp)\n"
		"lw a6,  4 * 16(sp)\n"
		"lw a7,  4 * 17(sp)\n"
		"lw s0,  4 * 18(sp)\n"
		"lw s1,  4 * 19(sp)\n"
		"lw s2,  4 * 20(sp)\n"
		"lw s3,  4 * 21(sp)\n"
		"lw s4,  4 * 22(sp)\n"
		"lw s5,  4 * 23(sp)\n"
		"lw s6,  4 * 24(sp)\n"
		"lw s7,  4 * 25(sp)\n"
		"lw s8,  4 * 26(sp)\n"
		"lw s9,  4 * 27(sp)\n"
		"lw s10, 4 * 28(sp)\n"
		"lw s11, 4 * 29(sp)\n"
		"lw sp,  4 * 30(sp)\n"
		"sret\n"
	);
}
```

Nuova versione:

Nella versione precedente, il gestore delle eccezioni si limitava ad segnalare un'eccezione e non a gestirle effettivamente.

Quindi in quel caso se scattava un eccezione il kernel andava in `PANIC`.

Ora invece dobbiamo fare in modo di distinguere tra un **errore** e un **evento** **normale** (ad esempio il *timer*).

```c
__asm__ __volatile__(
    "csrw sscratch, %[sscratch]\n"
    :
    : [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
);
```

Scrive nel registro `sscratch` l'indirizzo della CIMA dello stack del prossimo processo.

Questo serve per la gestione delle eccezioni. Quando il processo `next` sarà in esecuzione, potrebbe verificarsi un interrupt. Quando scatta un interrupt, la CPU salta in `kernel_entry`.

E la prima cosa che fa è:

```asm
csrw sscratch, sp  ; Scambia sp con sscratch
```

Poiché il puntatore dello stack si estende verso gli indirizzi più bassi, impostiamo l'indirizzo alla `sizeof(next->stack)`, come valore iniziale dello stack del kernel.

Nella nuova implementazione cambia il modo di operare.

- Prima:
  
  ```c
  csrw sscratch, sp       ; Salva SP in sscratch (backup)
  addi sp, sp, -4 * 31    ; Alloca spazio sullo stack ATTUALE
  ```

  Usava lo stesso stack su cui stava girando il programma interrotto.

- Dopo:
  
  ```c
  csrrw sp, sscratch, sp  ; SWAP atomico tra sp e sscratch
  ```

  Questa istruzione scambia il contenuto di `sp` e `sscratch` in un colpo solo.

  - prima dell'eccezione `sp` puntava allo stack del processo, e `sscratch` conteneva l'indirizzo dello stack del kernel.
  - dopo l'istruzione `sp` punta al kernel stack, e `sscratch` contiene il vecchio user stack
  
  Il risultato sta che non appena entriamo in kernel mode, stiamo già lavorando su una memoria sicura e dedicata al kernel.

La seconda fase consiste nel salvataggio del vecchio `sp` (user).

```c
csrr a0, sscratch       ; Legge il vecchio SP (che ora è in sscratch dopo lo swap)
sw a0,  4 * 30(sp)      ; Lo salva nel Trap Frame
```

Qui stiamo salvando nello stack del kernel il valore che lo stack pointer aveva prima dell'interruzione, questo è fondamentale per poter tornare al processo utente più tardi.

Infine abbiamo la preparazione per una prossima eccezione, ovvero viene riposizionato l'indirizzo del fondo del kernel stack all'interno di `sscratch`.

## page table

Fino a questo momento i processi hanno la possibilità di leggere e scrivere nello spazio di memoria del kernel. È davvero poco sicuro ciò.

Dobbiamo implementare quindi un meccanismo che ci permetta di isolare la memoria dedicata ai processi in modo che questi non possano accedere a quella di competenza degli altri processi e del kernel stesso.

Quando un processo accede alla memoria, la CPU traduce l'indirizzo specificato in un indirizzo fisico. La tabella che mappa gli indirizzi virtuali agli indirizzi fisici è chiamata *page table*.

Cambiando la tabella delle pagine, lo stesso indirizzo virtuale può corrispondere a diversi indirizzi fisici. Ciò consente l'isolamento degli spazi di indirizzamento e la separazione delle aree di memoria del kernel e delle applicazioni, migliorando la sicurezza del sistema.

### struttura dell'indirizzo virtuale

Il meccanismo di paging di RISC-V è chiamato Sv32, che utilizza una page table a due livelli. L'indirizzo virtuale a 32bit è suddiviso in un indice di tabella di pagina di primo livello, un indice di secondo livello e un offset di pagina.

[Come funzionano gli indirizzi virtuali in RISC-V](https://riscv-sv32-virtual-address.vercel.app/)

Questa struttura sfrutta il principio di località, permettendo page table più piccole e un uso efficiente della TLB.

Quando accede alla memoria, la CPU calcola `VPN[1]` e `VPN[2]` per identificare la corrispondente voce della tabella delle pagine, legge l'indirizzo fisico base mappato e aggiunge `offset` per ottenere l'indirizzo fisico finale.

### costruzione dalla tabella delle pagine

Costruiamo una tabella delle pagine in Sv32. Per prima cosa, definiremo alcune macro come:

- `SATP_SV32` è un singolo bit nel registro `SATP` che indica "abilita la paginazione in modalità Sv32", e `PAGE_*` sono flag da impostare nelle voci della page table.

La funzione `map_page` ci permette di mappare una pagina all'interno della page table, ha come input: la tabella di pagina di primo livello, l'indirizzo virtuale e i flag di ingresso della tabella della pagina.

Come è suddiviso un indirizzo virtuale:

|Id tabella 1|Id tabella 2|offset|
|:---:|:---:|:---:|
|10bit|10bit|12bit|

La funzione `map_page` garantisce che esista la entry della page table di primo livello e poi impostando la entry della page table di secondo livello per mappare una pagina fisica.

Divide `paddr` per `PAGE_SIZE` perché la entry contiene il numero di pagina fisico, non l'indirizzo fisico stesso.

Tale funzione ha il compito di dire alla CPU che l'indirizzo virtuale `x` corrisponde all'indirizzo fisico `y`.

- **Prima esegue un controllo di sicurezza**:

```c
if (!is_aligned(vaddr, PAGE_SIZE)){
    PANIC("unaligned vaddr %x", vaddr);
}

if (!is_aligned(paddr, PAGE_SIZE)){
    PANIC("unaligned vaddr %x", paddr);
}
```

Controlla che entrambi gli indirizzi siano multipli di 4KB. Perché la paginazione lavora su blocchi interi.

Non è possibile mappare pagine a metà. Un indirizzo di pagina deve finire sempre con tre zeri esadecimali.

- **Calcolo dell'indice di livello 1**

Estrae i primi 10 bit significativi dell'indirizzo virtuale, tali bit indicano il numero di pagina.

```c
uint32_t vpn1  = (vaddr >> 22) & 0x3ff;
```

Tale valore indica in che riga della page table principale è posizionata la entry corrispondente alla page table di secondo livello che mantiene la corrispondenza tra pagina virtuale e pagina fisica.

- **Controllo e creazione della page table di secondo livello**

```c
if ((table1[vpn1] & PAGE_V) == 0){
```

Guarda nella page table principale alla riga `vpn1` e controlla se il bit `PAGE_V` è 0; controlla se è valida o non.

Se è pari ad `1` allora la tabella di secondo livello esiste già, altrimenti se pari a `0` allora dobbiamo creare la entry per la tabella di secondo livello perché questa non esiste.

- **Allocazione della nuova tabella (se il validy bit è pari a 0)**

```c
uint32_t pt_paddr = alloc_pages(1);
```

Chiede all'allocatore che abbiamo realizzato di restituire una pagina di RAM da utilizzare come tabella.

`pt_paddr` contiene l'indirizzo fisico di questa nuova pagina

- **Collegamento nella tabella 1**

```c
table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
}
```

Scrive nella tabella principale il collegamento alla nuova tabella appena creata.

- `pt_paddr / PAGE_SIZE`: ottiene il PPN (physical page number). Toglie gli ultimi 12 bit (zeri).
- `<<10`: sposta il PPN nella posizione corretta, i 10bit bassi sono riservati ai flag.
- `| PAGE_V` accende il bit *validy*.
- Ora la tabella 1 sa che per quell'indirizzo virtuale deve andare a guardare nella nuova tabella fisica che abbimo appena allocato.

- **Calcolo dell'indice di livello 2**

```c
uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
```

Estrae i secondi 10 bit dell'indirizzo virtuale.

- **Trova l'indirizzo della tabella 2**

```c
uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
```

- **La mappatura finale**

```c
table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
```

Scrive l'indirizzo fisico di destinazione nella tabella di secondo livello.

- `paddr / PAGE_SIZE`: prende il PPN dei dati veri
- `<<10`: lo sposta in alto per far spazio ai flag
- `| flags`: aggiunge i permessi richiesti (rwx u)
- `| PAGE_V`: aggiunge il bit di validità


---

La tabella delle pagine deve esser configurata non solo per le applicazioni ma anche per lo stesso kernel.

In questo caso facciamo in modo che gli indirizzi virtuali del kernel corrispondono a quelli fisici (`vaddr == paddr`). Questo permette allo stesso codice di continuare a funzionare anche dopo aver abilitato la paginazione.

---

Modifichiamo il linker script per definire l'indirizzo di partezza usato dal kernel (`__kernel_base`):

```linker
ENTRY(boot)

SECTIONS {
    . = 0x80200000;
    __kernel_base = .;
```

---

Successivamente, si aggiunge la page table alla struttura del processo. Questo sarà un riferimento alla page table di primo livello.

---

Mappiamo le pagine del kernel nella funzione `create_process`, quelle relative al kernel per ogni processo. Le pagine del kernel vanno da `__kernel_base` a `__free_ram_end`. Questo approccio garantisce che il kernel possa sempre accedere sia alle aree allocate staticamente (come `.text`) sia alle aree allocate dinamicamente gestite da `alloc_pages`.

```c
extern char __kernel_base[];

struct process *create_process(uint32_t pc) {
    /* omitted */

	// NEW

    // Map kernel pages.
    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
	// END NEW

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table; // NEW
    return proc;
}
```

### switching page tables

Cambiamo la page table del processo quando si esegue un cambio di contesto.

```c
void yield(void) {
    /* omitted */

    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        // Don't forget the trailing comma!
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    switch_context(&prev->sp, &next->sp);
}
```

Possiamo cambiare la tabella delle pagine specificando la page table di primo livello in `SATP`. Nota che dividiamo per `PAGE_SIZE` perché è il numero di pagina fisico.

Adesso quando si cambia processo si deve cambiare anche la mappa della memoria.

```c
csrw satp, %[satp]
```

Il registro `satp` è il registro più importante per la memoria virtuale in RISC-V. Contiene l'indirizzo della page table di primo livello del processo corrente.

Appena scriviamo in questo registro, la CPU smette di usare la vecchia tabella delle pagine del processo `prev` e inizia a usare quella del nuovo processo `next`.

L'effetto sta che cambia il mapping degli stessi indirizzi virtuali. In questo modo possiamo utilizzare per entrambi i processi gli stessi indirizzi virtuali ma questi saranno mappati in posizioni diverse all'interno della memoria fisica.

```c
SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)
```

Prende l'indirizzo fisico della tabella e lo divide per `PAGE_SIZE`. Questo ci dà il PPN. Il registro `satp` infatti desidera il PPN e non l'indirizzo fisico crudo.

`| SATP_SV32`: attiva il bit 31 (il bit `MODE`). Questo accende la Memory Management Unit (MMU). Senza questo bit, la paginazione sarebbe spenta e gli indirizzi fisici sarebbero uguali a quelli virtuali.

- `sfence.vma` (pulizia della cache)
  
  lo eseguiamo due volte, prima e dopo il cambio di `satp`.

  Il problema: la traduzione degli indirizzi virtuali è lenta perché richiede di leggere la RAM due volte.

  Per essere veloce, la CPU ha una cache speciale chiamata TLB che ricorda le ultime traduzioni fatte.

  `sfence.vma` (supervisor fence virtual memory address) è un comando che dice alla CPU di dimenticare tutto quello che sa sugli indirizzi, ovvero che TLB non è più valida per le traduzioni.

  Si fa prima e dopo la scrittura di `satp` per garantire che nessuna operazione di memoria precedente o successiva usi traduzioni sbagliate.




