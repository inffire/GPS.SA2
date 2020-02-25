## GPS.SA.asi
Основаный на [примере](https://github.com/DK22Pac/plugin-sdk/tree/master/examples/GPS) от @DK22Pac плагин, строящий маршрут до цели на карте. В отличии от оригинала, работает не только в пределах зоны прорисовки. Строит маршрут по дорогам до конечной точки, удаленной на любое расстояние.

**Совместим с sa-mp и одиночной игрой.**

![](https://i.imgur.com/OsqcAdN.png)

## GPS.SA.xml
```
<?xml version="1.0"?>
<Color r="0" g="32" b="218" a="255" width="8" />
<Costs
	motorway="1.00999999"
	tertiary="1.24000001"
	residential="1.80999994"
	track="3.74000001"
	path="3.74000001"
	_reverse="227"
	trunk="1.5"
	primary="2"
	secondary="4"
	minor="125"
	service="30"
	unclassified="15"
/>
```
* **Color r, g, b** - цвет линии
* **width** - толщина линии
----
**Costs** хранит в себе коэффициенты дорог разных типов, представленных в файле **sanandreas.osm**. Чем меньше коэффициент, тем более предпочтительнее является дорога при поиске маршрута.
* **motorway, trunk, primary, secondary** - самые предпочитаемые основные магистрали (в порядке убывания)
* **residential, unclassified, service, minor** - городские дороги
* **track, path** - грунтовые дороги
* **_reverse** - движение "против шерсти" по дороге с односторонним движением
Подробнее здесь: https://wiki.openstreetmap.org/wiki/Key:highway#Values

## Ссылки
- https://github.com/DK22Pac/plugin-sdk
- https://github.com/OpenGameMaps/gtasagit
- https://github.com/zeux/pugixml

## Установка
в папку с gta_sa.exe поместить
- GPS.SA.asi
- GPS.SA.xml
- sanandreas.osm

## Компиляция
Создать новый плагин из шаблонов plugin_sdk или использовать любой его пример. Включить в проект файлы **pugixml**.

## Требования
- ASI Loader

## Скриншоты

![](https://i.imgur.com/FfZwGiS.png)
> дистанция до цели

![](https://i.imgur.com/OsqcAdN.png)
> скоростной маршрут

![](https://i.imgur.com/7fwz5RB.png)
> альтернативный маршрут

![](https://i.imgur.com/ZlFaNw1.png)
> предпочтение отдается магистралям (настраивается)
