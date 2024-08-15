# sched

Repositorio para el esqueleto del [TP sched](https://fisop.github.io/website/tps/sched) del curso Mendez-Fresia de **Sistemas Operativos (7508) - FIUBA**

## Respuestas teóricas

Utilizar el archivo `sched.md` provisto en el repositorio

## Elección scheduler

Para elegir la política de scheduling agregar los siguientes flags al final de la compilación:

Round Robin:
```bash
USE_RR=true
```

Lottery Scheduler
```bash
USE_LOTT=true
```

## Compilar

```bash
make
```

## Pruebas

```bash
make grade
```

## Linter

```bash
$ make format
```

Para efectivamente subir los cambios producidos por el `format`, hay que `git add .` y `git commit`.
