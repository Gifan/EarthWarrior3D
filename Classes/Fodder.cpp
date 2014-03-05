//
//  Fodder.cpp
//  Moon3d
//
//  Created by Hao Wu on 2/27/14.
//
//

#include "Fodder.h"
#include "PublicApi.h"

bool Fodder::init()
{
    
    _Model = Sprite3D::create("fighter.obj", "fighter.png");
    if(_Model)
    {
        _Model->setScale(20);
        addChild(_Model);
        _Model->setRotation3D(Vertex3F(90,0,0));
        _radius=40;
        move(3.0,Point(0.0f,-visible_size_macro.height*1.5));
        //this->schedule(schedule_selector(Fodder::acrobacy) , 0.1, -1, 0.0);
        this->scheduleUpdate();
        return true;
    }
    return false;
}

void Fodder::move(float duration, const Point& position)
{
    MoveTo*  MoveToB = MoveTo::create(duration,position);
    this->runAction(MoveToB);
}
/*
void Fodder::acrobacy(float dt)
{
   
}
 */
void Fodder::update(float dt)
{
    this->setRotation3D(Vertex3F(0,smoothAngle,0));
    smoothAngle+=rollSpeed;
}
