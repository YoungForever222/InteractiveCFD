#pragma once 
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <string>
#include <iostream>
#include <vector>
#include "RectFloat.h"
#include "RectInt.h"

class Color
{
public:
	enum ColorName{WHITE,BLACK,RED,GREEN,BLUE,DARK_GRAY,GRAY,LIGHT_GRAY};
	float r = 1.f;
	float g = 1.f;
	float b = 1.f;
	Color();
	Color(ColorName color);
};

class Button;
class Slider;

class Panel
{
public:
	enum SizeDefinitionMethod {DEF_ABS, DEF_REL};
	std::string m_name;
	std::vector<Panel*> m_subPanels;
	std::vector<Button*> m_buttons;
	std::vector<Slider*> m_sliders;
	Panel* m_parent = NULL; //pointer to parent frame
	RectInt m_rectInt_abs; //absolute coordinates in Window
	RectInt m_rectInt_rel; //relative coordinates wrt to parent
	RectFloat m_rectFloat_abs; //absolute coordinates in window. this is the one used for drawing, so always want to keep this up-to-date.
	RectFloat m_rectFloat_rel;
	Color m_backgroundColor;
	Color m_foregroundColor = Color::WHITE;
	bool m_draw = true;
	void(*m_callBack)() = NULL;
	std::string m_displayText = "";
	//these two members below should ideally be in Slider class
	float m_minValue = 1.f;
	float m_maxValue = 0.f;

	Panel();
	Panel(RectInt rectInt  , SizeDefinitionMethod sizeDefinition, std::string name, Color color, Panel* parent = NULL);
	Panel(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color, Panel* parent = NULL);

	Panel* GetRootPanel();
	Panel* GetPanel(std::string name);
	Button* GetButton(std::string name);
	Slider* GetSlider(std::string name);

	void CreateSubPanel(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color);
	void CreateSubPanel(RectInt rectInt    , SizeDefinitionMethod sizeDefinition, std::string name, Color color);
	void CreateButton(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color);
	void CreateButton(RectInt rectInt    , SizeDefinitionMethod sizeDefinition, std::string name, Color color);
	void CreateSlider(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color);

	RectFloat RectIntAbsToRectFloatAbs();
	RectFloat RectFloatRelToRectFloatAbs();

	void Draw(); //draw current panel only
	void DrawAll(); //draw current panel, then invoke DrawAll on immediate children. Effectively draws all subpanels

	virtual void Drag(float dx, float dy);
	virtual void Click();
};

class Button : public Panel
{
public:
	bool m_highlighted = false;
	
	using Panel::Panel;
	Button(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color, Panel* parent = NULL);
	Button(RectInt rectInt    , SizeDefinitionMethod sizeDefinition, std::string name, Color color, Panel* parent = NULL);

	virtual void Click();
};

class Slider;

class SliderBar : public Panel
{
public:
	enum Orientation {VERTICAL, HORIZONTAL};
	Orientation m_orientation = VERTICAL;
	//float m_maxValue = 1.f;
	//float m_minValue = 0.f;
	float m_value = 0.5f;

	SliderBar();
	SliderBar(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color, Slider* parent = NULL);
	SliderBar(RectInt rectInt    , SizeDefinitionMethod sizeDefinition, std::string name, Color color, Slider* parent = NULL);

	void UpdateValue();
	float GetValue();
	virtual void Drag(float dx, float dy);
};

class Slider : public Panel
{
public:
	float m_currentValue;
	SliderBar* m_sliderBar1 = NULL;
	SliderBar* m_sliderBar2 = NULL;

	Slider(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color, Panel* parent = NULL);
	Slider(RectInt rectInt    , SizeDefinitionMethod sizeDefinition, std::string name, Color color, Panel* parent = NULL);

	void CreateSliderBar(RectFloat rectFloat, SizeDefinitionMethod sizeDefinition, std::string name, Color color);

	void DrawAll();
	void Hide();
	void Show();
};

class ButtonGroup
{
public:
	std::vector<Button*> m_buttons;
	ButtonGroup();
	ButtonGroup(std::vector<Button*> &buttons);

	void ExclusiveEnable(Button* button);
	Button* GetCurrentEnabledButton();
};




Panel* GetPanelThatPointIsIn(Panel* parentPanel, float x, float y);
//Button* GetButtonThatPointIsIn(Button* parentPanel, float x, float y);
//SliderBar* GetSliderBarThatPointIsIn(SliderBar* parentPanel, float x, float y);

