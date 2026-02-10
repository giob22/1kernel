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

