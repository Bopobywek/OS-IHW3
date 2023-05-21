# Индивидуальное домашнее задание №3
<!-- omit from toc -->
#### Студент: Нечесов Андрей Львович
<!-- omit from toc -->
#### Группа: БПИ217
<!-- omit from toc -->
#### Вариант: 17


<!-- omit from toc -->
## Содержание
- [Условие задачи](#условие-задачи)
- [Сценарий решаемой задачи](#сценарий-решаемой-задачи)
  - [Устройство сервера и модель его взаимодействия с клиентами](#устройство-сервера-и-модель-его-взаимодействия-с-клиентами)
  - [Модель уведомления наблюдателей о событиях](#модель-уведомления-наблюдателей-о-событиях)
  - [Завершение сервера и клиентов](#завершение-сервера-и-клиентов)
  - [Ограничения](#ограничения)
- [Формат запуска](#формат-запуска)
  - [Программы на 4-5 баллов](#программы-на-4-5-баллов)
  - [Программы на 6-10 баллов](#программы-на-6-10-баллов)
- [Критерии на 4-10 баллов](#критерии-на-4-10-баллов)
  - [Демонстрация работы программы](#демонстрация-работы-программы)
  - [Клиенты-наблюдатели](#клиенты-наблюдатели)
- [Реакция сервера на отключение/переподключение садовника](#реакция-сервера-на-отключениепереподключение-садовника)
- [Реакция клиентов на завершение работы сервера](#реакция-клиентов-на-завершение-работы-сервера)


## Условие задачи
>Имеется пустой участок земли (двумерный массив) и план сада, разбитого на отдельные квадраты. От 10 до 30 процентов площади сада заняты прудами или камнями. То есть недоступны для ухаживания. Эти квадраты располагаются на плане произвольным образом. Ухаживание за садом выполняют два садовника, которые не хотят встречаться друг другом (то есть, одновременно появляться в одном и том же квадрате). Первый садовник начинает работу с верхнего левого угла сада и перемещается слева направо, сделав ряд, он спускается вниз и идет в обратном направлении, пропуская обработанные участки. Второй садовник начинает работу с нижнего правого угла сада и перемещается снизу вверх, сделав ряд, он перемещается влево и также идет в обратную сторону. Если садовник видит, что участок сада уже обработан другим садовником или является необрабатываемым, он идет дальше. Если по пути какой-то участок занят другим садовником, то садовник ожидает когда участок освободится, чтобы пройти дальше. Садовники должны работать одновременно со скоростями, определяемыми как параметры задачи. Прохождение через любой квадрат занимает некоторое время, которое задается константой, меньшей чем времена обработки и принимается за единицу времени. Создать приложение, моделирующее работу садовников. Каждого садовника представить отдельным клиентом. Сам сад &mdash; сервер.

## Сценарий решаемой задачи
### Устройство сервера и модель его взаимодействия с клиентами
Сервер держит два порта. Первый из них предназнчен для подключения клиентов-садовников, а второй &mdash; для подключения клиентов-наблюдателей.  

При запуске сервера создаются два потока: поток-писатель и поток-регистратор. Поток-писатель обрабатывает события, отправленные в неимнованный `pipe` (подробнее данная модель описывается в подраделе ниже), печатает их на экран, а также рассылает всем зарегистрированным клиентам-наблюдателям. Поток-регистратор принимает подключения от клиентов-наблюдателей и добавляет их в список подписчиков, которым будут рассылаться информация о состоянии системы. Список является массивом в разделяемой памяти.  

После того, как сервер запустился, ожидаются подключения клиентов-садовников. При подключении клиента-садовника создается дочерний, которому делегируется работа по обработке нового подключения.

В рамках подключения клиент-садовник отправляет на сервер сериализованную структуру, которая представляет задачу на выполнение. Структура содержит идентификатор садовника, координаты поля для обработки, время работы садовника, а также статус (закончил ли садовник работу). Если указанное поле не занято, садовнику позволяется начать работу на нем. Процесс моделирования аналогичен процессу, описанному в отчете к [ИДЗ-2](https://github.com/Bopobywek/OS-IHW2).

Ниже представлена обобщенная схема устройства сервера и его взаимодействия с клиентами:

<p align="center">
<img src="./pictures/common_schema.png">
</p>

### Модель уведомления наблюдателей о событиях
Любой процесс или поток может оповестить систему о событии следующем образом:
1. Создается и заполняется структура Event. Указывается тип события, текстовая информация к событию и timestamp &mdash; время, в которое данное событие произошло.
2. Созданное событие записывается в неименованный канал

Далее записанные в канал события читает специальный поток, который:
1. Выводит в консоль сервера все события с типом `MAP` (изменилось состояние сада) и `SERVER_INFO` (событие связано с работой сервера, например, подключился новый клиент)
2. Обходит массив наблюдателей и, если тот не отключился, отправляет каждому из них данное событие.  

Ниже представлена схема к данной модели:
<p align="center">
<img src="./pictures/notification_schema.png">
</p>

### Завершение сервера и клиентов
Завершение выполняется с помощью отправки сигнала `SIGINT` нажатием `Ctrl-C`. Обработчики данного сигнала закрывают все необходимые ресурсы: разделяемую память, сокеты, дожидаются завершения дочерних процессов

### Ограничения
Количество возможных клиентов-наблюдателей не может быть больше 100.  

Максимальный размер сада составляет `10x10` (как и в ИДЗ-2, задаются большие квадраты, а значит, настоящий размер двумерного массива равен `20x20`).  

Минимальный размер сада составляет `2x2`.  

## Формат запуска 
### Программы на 4-5 баллов
Запуск сервера:
```console
./server <Server Address> <Server Port> <Square side size>
```
Запуск садовников
```console
./first_gardener <Server Address> <Server Port> <Working time>
./second_gardener <Server Address> <Server Port> <Working time>
```
### Программы на 6-10 баллов
Запуск сервера:
```console
./server <Server Address> <Server Port> <Observer Port> <Square side size>
```
Запуск садовников
```console
./first_gardener <Server Address> <Server Port> <Working time>
./second_gardener <Server Address> <Server Port> <Working time>
```
Запуск наблюдателя
```console
./observer <Server address> <Server port>
```
## Критерии на 4-10 баллов
Так как программы на каждую из оценок являются дополнением друг к другу, в данном разделе демонстрируется соответствие [программ](./6-10) на 10 баллов всем поставленным требованиям.
### Демонстрация работы программы
Сервер запускался на виртуальной машине под операционной системой Ubuntu в Яндекс.Облаке, клиенты &mdash; на домашнем компьютере. Система успешно отработала в данном сценарии. Ниже приведен результат работы:
<p align="center">
  <img src="./pictures/demo-1.png">
</p>

Слева показан вывод клиентов-садовников, которые были одновременно запущены при помощи скрипта [run.sh](./6-10/run.sh), справа &mdash; вывод на сервере (было осуществлено подключение по `ssh`).  


### Клиенты-наблюдатели
Для мониторинга данных была написана программа [observer.c](./6-10/observer.c). К серверу может подключиться до 100 наблюдателей. 

Ниже приведено два скриншота с демонстрацией работы клиента-наблюдателя. Сначала запускается сервер, затем подключается наблюдатель, после чего начинают работу садовники. У наблюдателя отображается информация о моделировании как с сервера, так и с клиентов-садовников.
<p align="center">
  <img src="./pictures/demo-2.png">
</p>
<p align="center">
<em>Запуск сервера и клиентов.</em>
</p>  
  

<p align="center">
  <img src="./pictures/demo-3.png">
</p>
<p align="center">
<em>Демонстрация работы наблюдателя.</em>
</p>  

<p align="center">
  <img src="./pictures/demo-4.png">
</p>
<p align="center">
<em>Демонстрация подключения нескольких наблюдателей.</em>
</p> 

На рисунке ниже "Наблюдатель 2" отключается, а затем подключается снова. Работа сервера и других клиентов не нарушается.

<p align="center">
  <img src="./pictures/demo-5.png">
</p>
<p align="center">
<em>Демонстрация переподключения одного из наблюдателей.</em>
</p> 

## Реакция сервера на отключение/переподключение садовника
<p align="center">
  <img src="./pictures/demo-6.png">
</p>
<p align="center">
<em>Демонстрация переподключения одного из садовников.</em>
</p> 

## Реакция клиентов на завершение работы сервера
