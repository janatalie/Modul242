
#include <Wire.h>
#include <Zumo32U4.h>
#include "GridMovement.h"

Zumo32U4LCD lcd;
Zumo32U4Buzzer buzzer;
Zumo32U4ButtonA buttonA;
Zumo32U4Motors motors;
Zumo32U4LineSensors lineSensors;
L3G gyro;

// Die path variable speichert den Weg, welcher der roboter schon gemacht hat.
// Es ist ein array von Charakteren, welche bestimmen, in welcher Richtung der Roboter sich gedreht hat.
// Die characteren sind:
//  'L' für links
//  'R' für rechts
//  'S' für gerade aus
//  'B' für zurück (108 grad drehen)
//
// Die simplifyPath() Funktion prüft dead ends und entfernt sie von der "path" variable immer, wann der roboter sich dreht.
char path[100];
uint8_t pathLength = 0;

void setup()
{
  buzzer.playFromProgramSpace(PSTR("!>g32>>c32"));

  gridMovementSetup();

  // mazeSolve() prüft jede möglichteit der Labyrinth zu lösen und findet den optimalsten Weg.
  mazeSolve();
}

void loop()
{
  // Der Labyrinth wurde gelöst. Wart auf dem Benutzer, um dem Knopf zu drücken.
  buttonA.waitForButton();

  // Folg den optimalsten Weg, der vorher gefunden wurde.
  mazeFollowPath();
}

// Diese funktion entscheidet in welcher Richtung der Roboter drehen sollte.
// Es benutzt die Variabeln found_left, found_straight, und found_right die bestimmen, 
// ob es möglich ist nach Links, Geradeaus oder Rechts weiter zu gehen, indem die
// Strategie left-hand-on-the-wall benutzt.
char selectTurn(bool foundLinks, bool foundGeradeaus, bool foundRechts)
{
  // Die left-hand-on-the-wall strategy ist, wann der roboter immer zuerst
  // nach Links dreht, wann es möglich ist und dann die andere möglichkeiten benutzt.
  if(foundLinks) { return 'L'; }
  else if(foundGeradeaus) { return 'S'; }
  else if(foundRechts) { return 'R'; }
  else { return 'B'; }
}

void mazeSolve()
{
  // path länge ist im Anfang immer 0, da noch nichts gelöst wurde. 
  pathLength = 0;

  // Ein Ton im Anfang abspielen.
  buzzer.playFromProgramSpace(PSTR("!L16 cdegreg4"));

  // Kurze Pause, damit der Roboter nicht schon fährt, während der Benutzer den Knopf drückt.
  delay(1000);

  while(1)
  {
    // Fährt gerade, bis es möglich ist in einer Richtung zu drehen..
    followSegment();

    // Prüft ob es möglkich ist, nach Rechts oder Links zu drehen.
    bool foundLeft, foundStraight, foundRight;
    driveToIntersectionCenter(&foundLeft, &foundStraight, &foundRight);

    if(aboveDarkSpot())
    {
      // Die Ende wurde gefunden und die Aufgabe ist erledigt
      break;
    }

    // Wäht eine Richtung zu drehen
    char dir = selectTurn(foundLeft, foundStraight, foundRight);

    // Macht es sicher, dass die max Länge von pathLength nicht überschrieten wird, 
    // damit kein unerwarteter Fehler passiert
    if (pathLength >= sizeof(path))
    {
      lcd.clear();
      lcd.print(F("pathfull"));
      while(1)
      {
        ledRed(1);
      }
    }

    // Speichert die ausgewählte Richtung
    path[pathLength] = dir;
    pathLength++;

    // Vereinfacht die Lösung
    simplifyPath();

    // Zeig den Weg auf dem Display
    displayPath();
    
    // Wenn der Weg "BB" ist, heisst es, dass der Roboter kein weg gefunden hat.
    // Der roboter gibt ein ton aus, aber sucht weiter, da es sein kann,
    // dass der Sensor etwas verpasst hat
    if (pathLength == 2 && path[0] == 'B' && path[1] == 'B')
    {
      buzzer.playFromProgramSpace(PSTR("!<b4"));
    }

    // Dreht 180 Grad
    turn(dir);
  }
  
  // Der optimalste Weg wurde gefunden. Der Roboter gibt ein ton aus und
  // der Motor wird ausgeschaltet.
  motors.setSpeeds(0, 0);
  buzzer.playFromProgramSpace(PSTR("!>>a32"));
}

// Folgt den optmalsten weg
void mazeFollowPath()
{
  // Ein Ton im Anfang abspielen.
  buzzer.playFromProgramSpace(PSTR("!>c32"));

  // Kurze Pause, damit der Roboter nicht schon fährt, während der Benutzer den Knopf drückt.
  delay(1000);

  for(uint16_t i = 0; i < pathLength; i++)
  {
    //Fährt bis zu der Mitte vom Weg
    followSegment();
    driveToIntersectionCenter();

    // Dreht 180 Grad
    turn(path[i]);
  }

  // Färt der rest von den Weg.
  followSegment();

  // Die Ende wurde erreicht. Der Roboter gibt ein ton aus und
  // der Motor wird ausgeschaltet.
  motors.setSpeeds(0, 0);
  buzzer.playFromProgramSpace(PSTR("!>>a32"));
}

// Weg Vereinfachung.
// Die strategie ist, immer wann ein Sequenz xBx gefunden wird, wird es gelöscht.
// Zum Beispiel LBL = S, so wird den falschen Weg ignoriert.
void simplifyPath()
{
  if(pathLength < 3 || path[pathLength - 2] != 'B')
  {
    return;
  }

  int16_t totalAngle = 0;

  for(uint8_t i = 1; i <= 3; i++)
  {
    switch(path[pathLength - i])
    {
    case 'L':
      totalAngle += 90;
      break;
    case 'R':
      totalAngle -= 90;
      break;
    case 'B':
      totalAngle += 180;
      break;
    }
  }

  // Speichert der Winkel, der gedreht wurde, von 0 zu 360.
  totalAngle = totalAngle % 360;

  switch(totalAngle)
  {
  case 0:
    path[pathLength - 3] = 'S';
    break;
  case 90:
    path[pathLength - 3] = 'L';
    break;
  case 180:
    path[pathLength - 3] = 'B';
    break;
  case 270:
    path[pathLength - 3] = 'R';
    break;
  }

  // The path is now two steps shorter.
  pathLength -= 2;
}

void displayPath()
{
  // Gibt den gespeicherten Weg aus.
  path[pathLength] = 0;

  lcd.clear();
  lcd.print(path);
  if(pathLength > 8)
  {
    lcd.gotoXY(0, 1);
    lcd.print(path + 8);
  }
}
