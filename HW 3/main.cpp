#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <vector>
#include <iterator>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cmath>
#include <random>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

GLFWwindow* window;

#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/objloader.hpp>
#include <common/text2D.hpp>

static const int ShaderNum = 25;
static const int h = 768;
static const int w = 1024;
static const GLfloat time_coef = 10.0f;
static const GLfloat max_distance = 300.0f;

class LoadedModel {
public:
	explicit LoadedModel(const std::string& obj_file, const std::string& texture_file) {
		texture_ = loadDDS(texture_file.data());
		loadOBJ(obj_file.data(), vertices_, uvs_, normals_);
	}
	virtual ~LoadedModel() {
		glDeleteTextures(1, &texture_);
	}

	void DrawShader(GLuint texture_id, GLuint vertexbuffer, GLuint uvbuffer, GLuint normalbuffer) {
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(glm::vec3), &vertices_[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glBufferData(GL_ARRAY_BUFFER, uvs_.size() * sizeof(glm::vec3), &uvs_[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glBufferData(GL_ARRAY_BUFFER, normals_.size() * sizeof(glm::vec3), &normals_[0], GL_STATIC_DRAW);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_);
		glUniform1i(texture_id, 0);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glDrawArrays(GL_TRIANGLES, 0, vertices_.size());

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}

	virtual void Draw(GLuint texture_id, GLuint vertexbuffer, GLuint uvbuffer, GLuint normalbuffer) {
		DrawShader(texture_id, vertexbuffer, uvbuffer, normalbuffer);
	}

protected:
	GLuint texture_;
	std::vector<glm::vec3> vertices_;
	std::vector<glm::vec2> uvs_;
	std::vector<glm::vec3> normals_;
};

class Camera {
public:
	explicit Camera(GLfloat horizontal_angle = 0.0f, GLfloat vertical_angle = 0.0f, GLfloat fov = 45.0f)
		: horizontal_angle_(horizontal_angle), vertical_angle_(vertical_angle), fov_(fov) {}

	void Save(std::iostream& file) {
		file << horizontal_angle_ << " " << vertical_angle_ << " " << fov_ << std::endl;
	}

	void Load(std::iostream& file) {
		file >> horizontal_angle_ >> vertical_angle_ >> fov_;
	}

	virtual ~Camera() = default;

	virtual glm::vec3 CameraDirection() {
		return glm::vec3(
			cos(vertical_angle_) * sin(horizontal_angle_),
			sin(vertical_angle_),
			cos(vertical_angle_) * cos(horizontal_angle_)
		);
	}

	virtual glm::vec3 CameraRight() {
		return glm::vec3(
			sin(horizontal_angle_ - 3.14f / 2.0f),
			0,
			cos(horizontal_angle_ - 3.14f / 2.0f)
		);
	}

	virtual glm::vec3 CameraUp() {
		return glm::cross(CameraRight(), CameraDirection());
	}

	virtual GLfloat FOV() {
		return fov_;
	}

protected:
	GLfloat horizontal_angle_;
	GLfloat vertical_angle_;
	GLfloat fov_;
};

class Object : public LoadedModel {
public:
	explicit Object(const glm::vec3& position, const glm::vec3& direction,
		GLfloat box, GLfloat speed,
		const std::string& obj_file, const std::string& texture_file)
		: position_(position), direction_(direction), box_(box), speed_(speed), 
		LoadedModel(obj_file, texture_file) {}

	virtual void Save(std::iostream& file) {
		file << position_.x << " " << position_.y << " " << position_.z << std::endl;
		file << direction_.x << " " << direction_.y << " " << direction_.z << std::endl;
		file << box_ << " " << speed_ << std::endl;
	}

	virtual void Load(std::iostream& file) {
		file >> position_.x >> position_.y >> position_.z;
		file >> direction_.x >> direction_.y >> direction_.z;
		file >> box_ >> speed_;
	}

	virtual ~Object() = default;
	virtual bool Interract(Object* obj, const glm::vec3& old_position) = 0;
	virtual bool CheckInterraction(Object* obj) {
		return glm::length(position_ - obj->Position()) < (box_ + obj->Box());
	}
	glm::vec3 Position() { return position_; }
	virtual bool CheckSelf() { return true; }
	virtual glm::vec3 GetDirection() { return direction_;  };
	float GetSpeed() { return speed_; }
	void Move(const glm::vec3& move) { position_ += move; }
	virtual Object* Act(const std::vector<Object*>& objects) = 0;
	GLfloat Box() { return box_; }

protected:
	glm::vec3 position_;
	glm::vec3 direction_;
	GLfloat box_;
	GLfloat speed_;
};

class Floor : public Object {
public:
	explicit Floor(const glm::vec3& position, int repeats=10)
		: Object(position, glm::vec3(0.0f, 1.0f, 0.0f), 100.0f, 0.0f, "floor.obj", "new_floor.DDS") {

		size_t num = vertices_.size();
		for (int i = -repeats; i <= repeats; ++i) {
			for (int j = -repeats; j <= repeats; ++j) {
				for (size_t k = 0; k < num; ++k) {
					vertices_.push_back(vertices_[k] + glm::vec3(i, 0.0f, j) * 2.0f);
				}
			}
		}

		num = uvs_.size();
		for (int i = -repeats; i <= repeats; ++i) {
			for (int j = -repeats; j <= repeats; ++j) {
				for (size_t k = 0; k < num; ++k) {
					uvs_.push_back(uvs_[k]);
				}
			}
		}

		num = normals_.size();
		for (int i = -repeats; i <= repeats; ++i) {
			for (int j = -repeats; j <= repeats; ++j) {
				for (size_t k = 0; k < num; ++k) {
					normals_.push_back(normals_[k]);
				}
			}
		}
	}

	~Floor() override = default;
	virtual bool Interract(Object* obj, const glm::vec3& old_position) { return true; }
	virtual bool CheckInterraction(Object* obj) {
		glm::vec3 diff = obj->Position() - position_;

		return diff.y < obj->Box();
	}
	Object* Act(const std::vector<Object*>& objects) override {
		return nullptr;
	};
};

class Skybox : public Object {
public:
	explicit Skybox(const glm::vec3& position)
		: Object(position, glm::vec3(0.0f, 1.0f, 0.0f), max_distance, 0.0f, "skybox.obj", "skybox.DDS") {}

	void Save(std::iostream& file) override {
		Object::Save(file);
	}

	void Load(std::iostream& file) override {
		Object::Load(file);
	}

	~Skybox() override = default;
	virtual bool Interract(Object* obj, const glm::vec3& old_position) { return true; }
	virtual bool CheckInterraction(Object* obj) {
		glm::vec3 diff = obj->Position() - position_;

		return glm::length(diff) > box_ - obj->Box();
	}
	Object* Act(const std::vector<Object*>& objects) override {
		return nullptr;
	};
	void MoveTo(const glm::vec3& position) {
		position_ = position;
	}
};

class Actor : public Object {
public:
	explicit Actor(const glm::vec3& position, GLfloat box, GLfloat hp, GLfloat speed, 
		const glm::vec3& direction)
		: Object(position, direction, box, speed, "enemy.obj", "enemy.DDS"), hp_(hp) {}

	void Save(std::iostream& file) override {
		Object::Save(file);
		file << hp_ << std::endl;
	}

	void Load(std::iostream& file) override {
		Object::Load(file);
		file >> hp_;
	}

	~Actor() override = default;
	bool Interract(Object* obj, const glm::vec3& old_position) override;
	void ReceiveDamage(GLfloat damage) { this->hp_ -= damage; };
	void Die() { this->hp_ = -1.0f; };
	float HP() { return hp_; }
	Object* Act(const std::vector<Object*>& objects) override { return nullptr; }

protected:
	GLfloat hp_;
};

class Dummy : public Actor {
public:
	explicit Dummy(const glm::vec3& position, const glm::vec3& direction = glm::vec3(0.0f, 0.0f, 0.0f),
		GLfloat box = 1.0f, GLfloat hp = 1.0f)
		: Actor(position, box, hp, 0.0f, direction) {}
	~Dummy() override = default;
};

class Projectile : public Object {
public:
	explicit Projectile(const glm::vec3& position, const glm::vec3& direction, GLfloat box = 0.1f,
		GLfloat damage = 1.0f, GLfloat speed = 10.0f, GLfloat tl = 10.0f, GLfloat explode_speed = 10.0f,
		GLfloat explode_duration = 1.0f)
		: Object(position, direction, box, speed, "projectile.obj", "projectile.DDS"), 
		damage_(damage), end_time_(glfwGetTime() + tl), explode_speed_(explode_speed),
		explode_duration_(explode_duration) {}

	void Save(std::iostream& file) override {
		Object::Save(file);
		file << damage_ << " " << end_time_ << " " << exploded_ << " " << interracted_ << " " <<
			time_exploded_ << " " << explode_speed_ << " " << explode_duration_ << std::endl;
	}

	void Load(std::iostream& file) override {
		Object::Load(file);
		file >> damage_ >> end_time_ >> exploded_ >> interracted_ >> 
			time_exploded_ >> explode_speed_ >> explode_duration_;
	}

	~Projectile() override = default;
	bool Interract(Object* obj, const glm::vec3& old_position) override;
	GLfloat DealDamage(Object* obj) { return damage_; };
	bool CheckSelf() override { 
		if (!exploded_) {
			if ((glfwGetTime() > end_time_) || interracted_) {
				Explode();
			}
			return true;
		} else {
			return glfwGetTime() - time_exploded_ < explode_duration_;
		}
	}
	Object* Act(const std::vector<Object*>& objects) {
		return nullptr; 
	}
	GLfloat ExplodedMove() {
		if (exploded_) {
			return (glfwGetTime() - time_exploded_) * explode_speed_;
		}
		else {
			return 0.0f;
		}
	}
	void Explode() {
		exploded_ = true;
		direction_ = glm::vec3();
		time_exploded_ = glfwGetTime();
	}
	bool Exploded() {
		return exploded_;
	}

protected:
	GLfloat damage_;
	GLfloat end_time_;
	bool exploded_ = false;
	bool interracted_ = false;
	GLfloat time_exploded_ = 0.0f;
	GLfloat explode_speed_;
	GLfloat explode_duration_;
};

bool Actor::Interract(Object* obj, const glm::vec3& old_position) {
	if (CheckInterraction(obj) && obj->CheckInterraction(this)) {
		Projectile* proj = dynamic_cast<Projectile*>(obj);
		if (proj != nullptr) {
			if (!proj->Exploded()) {
				ReceiveDamage(proj->DealDamage(this));
			}
		}
		if (dynamic_cast<Actor*>(obj) != nullptr) {
			position_ = old_position;
		}
		if (dynamic_cast<Floor*>(obj) != nullptr) {
			position_ = old_position;
		}
		return hp_ > 0.0f;
	}
	else {
		return true;
	}
}

bool Projectile::Interract(Object* obj, const glm::vec3& old_position) {
	if (!exploded_) {
		if (dynamic_cast<Floor*>(obj) != nullptr) {
			if (obj->CheckInterraction(this)) {
				interracted_ = true;
			}
		}
		else {
			if (CheckInterraction(obj) && obj->CheckInterraction(this)) {
				if (dynamic_cast<Actor*>(obj) != nullptr) {
					interracted_ = true;
				}
			}
		}
	}
	return true;
}

class Player : public Actor, public Camera {
public:
	explicit Player(const glm::vec3& position = glm::vec3(0.0f, 2.0f, 0.0f), 
		GLfloat box = 1.0f, GLfloat hp = 10.0f, GLfloat speed=5.0f, GLfloat mouse_speed = 0.005f,
		GLfloat cooldown = 1.0f, size_t killed = 0)
		: Actor(position, box, hp, speed, glm::vec3(0.0f, 0.0f, 0.0f)), 
		mouse_speed_(mouse_speed), cooldown_(cooldown), killed_(killed) {}

	~Player() override = default;

	void Save(std::iostream& file) override {
		Actor::Save(file);
		Camera::Save(file);
		file <<  killed_ << " " << mouse_speed_ << " " << cooldown_ << " " << next_projectile_ << std::endl;
	}

	void Load(std::iostream& file) override {
		Actor::Load(file);
		Camera::Load(file);
		file >> killed_ >> mouse_speed_ >> cooldown_ >> next_projectile_;
	}

	Object* Act(const std::vector<Object*>& objects) override {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		glfwSetCursorPos(window, w / 2, h / 2);
		horizontal_angle_ += mouse_speed_ * GLfloat(w / 2 - xpos);
		vertical_angle_ += mouse_speed_ * GLfloat(h / 2 - ypos);

		if (vertical_angle_ > 3.14f / 2.0f) {
			vertical_angle_ = 3.14f / 2.0f;
		}

		if (vertical_angle_ < -3.14f / 2.0f) {
			vertical_angle_ = -3.14f / 2.0f;
		}

		glm::vec3 direction(
			sin(horizontal_angle_),
			0.0f,
			cos(horizontal_angle_)
		);

		glm::vec3 right = CameraRight();

		glm::vec3 final_direction = glm::vec3(0.0f, 0.0f, 0.0f);

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			final_direction += direction;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			final_direction -= direction;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			final_direction += right;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			final_direction -= right;
		}

		if (glm::length(final_direction) > 0.0f) {
			final_direction /= glm::length(final_direction);
		}

		direction_ = final_direction;

		glm::vec3 camera_direction = CameraDirection();

		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
			if (glfwGetTime() > next_projectile_) {
				next_projectile_ = glfwGetTime() + cooldown_;
				Projectile* new_proj = new Projectile(position_ + camera_direction * (box_ + 0.2f), 
					camera_direction, 0.1f, 1.0f);
				return new_proj;
			}
		}
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
			if (glfwGetTime() > next_projectile_) {
				next_projectile_ = glfwGetTime() + cooldown_;
				Projectile* new_proj = new Projectile(position_ + camera_direction * (box_ + 2.0f), 
					camera_direction, 1.0f, 2.0f, 1.0f, 20.0f);
				return new_proj;
			}
		}
		
		return nullptr;
	}

	size_t Killed() {
		return killed_;
	}

	void Kill() {
		++killed_;
	}

protected:
	size_t killed_;
	GLfloat mouse_speed_;
	GLfloat cooldown_;
	GLfloat next_projectile_ = glfwGetTime();
};

class Enemy : public Actor {
public:
	explicit Enemy(const glm::vec3& position, const glm::vec3& direction = glm::vec3(0.0f, 0.0f, 0.0f), 
		GLfloat box = 1.0f, GLfloat hp = 1.0f, GLfloat speed = 1.0f, GLfloat cooldown = 5.0f)
		: Actor(position, box, hp, speed, direction), cooldown_(cooldown) {}

	~Enemy() override = default;

	void Save(std::iostream& file) override {
		Actor::Save(file);
		file << cooldown_ << " " << next_projectile_ << std::endl;
	}

	void Load(std::iostream& file) override {
		Actor::Load(file);
		file >> cooldown_ >> next_projectile_;
	}

	Object* Act(const std::vector<Object*>& objects) override {
		Object* target = nullptr;

		for (Object* obj : objects) {
			Player* cast_obj = dynamic_cast<Player*>(obj);
			if (cast_obj != nullptr) {
				target = cast_obj;
				break;
			}
		}

		if (target != nullptr) {
			glm::vec3 direction = target->Position() - position_;
			if (glm::length(direction) > 0.0f) {
				direction /= glm::length(direction);
			}

			direction_ = direction;

			if (glfwGetTime() > next_projectile_) {
				next_projectile_ = glfwGetTime() + cooldown_;
				Projectile* new_proj = new Projectile(position_ + direction * (box_ + 0.2f),
					direction, 0.1f, 2.0f);
				return new_proj;
			}
		}

		return nullptr;
	}

protected:
	GLfloat cooldown_;
	GLfloat next_projectile_ = glfwGetTime() + cooldown_;
};

class EnemyCreator {
public:
	explicit EnemyCreator(GLfloat cooldown = 10.0f, size_t retries = 10, GLfloat p = 0.01, GLfloat r_from = 30.0f,
		GLfloat r_to = 50.0f, GLfloat hp_from = 1.0f, GLfloat hp_to = 5.0f, 
		GLfloat speed_from = 1.0f, GLfloat speed_to = 2.0f)
		: cooldown_(cooldown), rng_(std::random_device()()), retries_(retries), type_(p), angle_(-3.14, 3.14),
		r_(r_from, r_to), hp_(hp_from, hp_to), speed_(speed_from, speed_to)
	{}

	Object* CreateEnemy(const glm::vec3& position, const std::vector<Object*>& objects) {
		if (glfwGetTime() > next_creation_) {
			next_creation_ = glfwGetTime() + cooldown_;

			GLfloat angle_direction = angle_(rng_);
			glm::vec3 orientation(
				sin(angle_direction),
				0.0f,
				cos(angle_direction)
			);

			bool type = type_(rng_);

			for (size_t retry = 0; retry < retries_; ++retry) {
				GLfloat angle_position = angle_(rng_);
				glm::vec3 direction(
					sin(angle_position),
					0.0f,
					cos(angle_position)
				);

				GLfloat r = r_(rng_);

				glm::vec3 new_position = position + r * direction;

				Object* new_obj = nullptr;

				if (type) {
					new_obj = new Enemy(new_position, orientation, 1.0f, hp_(rng_), speed_(rng_));
				}
				else {
					new_obj = new Dummy(new_position, orientation, 1.0f, hp_(rng_));
				}

				bool possible = true;

				for (Object* obj : objects) {
					possible = possible && (!new_obj->CheckInterraction(obj) || !obj->CheckInterraction(new_obj));
				}

				if (possible) {
					return new_obj;
				}
				else {
					delete new_obj;
				}
			}
		}
		return nullptr;
	}
private:
	GLfloat cooldown_;
	size_t retries_;
	GLfloat next_creation_ = glfwGetTime();
	std::mt19937 rng_;
	std::bernoulli_distribution type_;
	std::uniform_real_distribution<> angle_;
	std::uniform_real_distribution<> r_;
	std::uniform_real_distribution<> hp_;
	std::uniform_real_distribution<> speed_;
};

void SaveToFile(const std::string& file, std::vector<Object*>& objects) {
	std::fstream fs;
	fs.open(file, std::fstream::out);
	fs << glfwGetTime() << std::endl;
	fs << objects.size() << std::endl;
	for (Object* obj : objects) {
		if (dynamic_cast<Player*>(obj) != nullptr) {
			fs << 0 << std::endl;
		}
		if (dynamic_cast<Dummy*>(obj) != nullptr) {
			fs << 1 << std::endl;
		}
		if (dynamic_cast<Enemy*>(obj) != nullptr) {
			fs << 2 << std::endl;
		}
		if (dynamic_cast<Projectile*>(obj) != nullptr) {
			fs << 3 << std::endl;
		}
		if (dynamic_cast<Floor*>(obj) != nullptr) {
			fs << 4 << std::endl;
		}
		obj->Save(fs);
	}
	fs.close();
}

void LoadFromFile(const std::string& file, std::vector<Object*>& objects, Player*& player, GLfloat& prev_time) {
	std::fstream fs;
	fs.open(file, std::fstream::in);

	if (fs.is_open()) {
		GLfloat time;
		fs >> time;

		size_t obj_size;
		fs >> obj_size;

		std::vector<Object*> new_objects;

		for (size_t i = 0; i < obj_size; ++i) {
			size_t type;
			fs >> type;

			Object* new_obj = nullptr;
			if (type == 0) {
				new_obj = new Player();
			}
			if (type == 1) {
				new_obj = new Dummy(glm::vec3());
			}
			if (type == 2) {
				new_obj = new Enemy(glm::vec3());
			}
			if (type == 3) {
				new_obj = new Projectile(glm::vec3(), glm::vec3());
			}
			if (type == 4) {
				new_obj = new Floor(glm::vec3());
			}
			new_obj->Load(fs);

			new_objects.push_back(new_obj);
		}
		fs.close();

		for (Object* old_obj : objects) {
			delete old_obj;
		}

		objects = new_objects;
		player = dynamic_cast<Player*>(objects[0]);

		glfwSetTime(time);
		prev_time = glfwGetTime();
	}
}

int main(void)
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(w, h, "Shooter", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


	glfwPollEvents();
	glfwSetCursorPos(window, w / 2, h / 2);

	glClearColor(0.05f, 0.05f, 0.05f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	GLuint programID = LoadShaders("TextureVertex.vertexshader", "TextureFragment.fragmentshader");
	GLuint simpleProgramID = LoadShaders("ColorVertex.vertexshader", "ColorFragment.fragmentshader");

	GLuint ProjectionID = glGetUniformLocation(programID, "Projection");
	GLuint ViewID = glGetUniformLocation(programID, "View");
	GLuint ModelID = glGetUniformLocation(programID, "Model");
	GLuint TextureID = glGetUniformLocation(programID, "TextureSampler");
	GLuint LightID = glGetUniformLocation(programID, "LightPositions_worldspace");
	GLuint LightColorID = glGetUniformLocation(programID, "LightColors");
	GLuint LightPowerID = glGetUniformLocation(programID, "LightPowers");
	GLuint AmbientID = glGetUniformLocation(programID, "Ambient");
	GLuint SpecularID = glGetUniformLocation(programID, "Specular");
	GLuint NumID = glGetUniformLocation(programID, "LightSources");

	GLuint SimpleTextureID = glGetUniformLocation(simpleProgramID, "TextureSampler");
	GLuint SimpleProjectionID = glGetUniformLocation(simpleProgramID, "Projection");
	GLuint SimpleViewID = glGetUniformLocation(simpleProgramID, "View");
	GLuint SimpleModelID = glGetUniformLocation(simpleProgramID, "Model");
	GLuint SimpleMoveID = glGetUniformLocation(simpleProgramID, "Move");

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);

	initText2D("Holstein.DDS");

	Player* player = new Player();
	Skybox* skybox = new Skybox(player->Position());

	std::vector<Object*> objects;
	objects.push_back(player);
	objects.push_back(new Floor(player->Position() - glm::vec3(0.0f, player->Position().y, 0.0f)));


	GLfloat prev_time = glfwGetTime();
	GLfloat timespeed = 1.0f;

	EnemyCreator enemy_creator;

	bool saved = false;
	bool loaded = false;

	do {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
			if (!saved) {
				SaveToFile("save.txt", objects);
				saved = true;
			}
		}
		if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_RELEASE) {
			saved = false;
		}

		if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
			if (!loaded) {
				LoadFromFile("save.txt", objects, player, prev_time);
				loaded = true;
			}
		}
		if (glfwGetKey(window, GLFW_KEY_X) == GLFW_RELEASE) {
			loaded = false;
		}

		skybox->MoveTo(player->Position());

		timespeed = 1.0f;

		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
			timespeed *= time_coef;
		}

		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
			timespeed *= 1.0f / time_coef;
		}

		GLfloat timediff = (glfwGetTime() - prev_time) * timespeed;
		glfwSetTime(prev_time + timediff);
		GLfloat current_time = glfwGetTime();

		std::vector<glm::vec3> old_positions;

		for (Object* obj : objects) {
			old_positions.push_back(obj->Position());
			obj->Move(obj->GetDirection() * obj->GetSpeed() * timediff);
		}

		prev_time = current_time;

		std::vector<bool> remains(objects.size(), true);

		for (size_t i = 0; i < objects.size(); ++i) {
			remains[i] = objects[i]->CheckSelf();
		}

		for (size_t i = 0; i < objects.size(); ++i) {
			for (size_t j = i + 1; j < objects.size(); ++j) {
				remains[i] = remains[i] && objects[i]->Interract(objects[j], old_positions[i]);
				remains[j] = remains[j] && objects[j]->Interract(objects[i], old_positions[j]);
			}
		}

		if (!remains[0]) {
			break;
		}

		std::vector<Object*> new_objects;

		for (size_t i = 0; i < objects.size(); ++i) {
			if (remains[i]) { 
				new_objects.push_back(objects[i]);
			}
			else {
				if (dynamic_cast<Dummy*>(objects[i]) != nullptr || dynamic_cast<Enemy*>(objects[i]) != nullptr) {
					player->Kill();
				}
				delete objects[i];
			}
		}

		std::vector<Object*> act_objects;

		for (size_t i = 0; i < new_objects.size(); ++i) {
			act_objects.push_back(new_objects[i]);
			Object* new_obj = new_objects[i]->Act(new_objects);
			if (new_obj != nullptr) {
				act_objects.push_back(new_obj);
			}
		}

		objects = act_objects;

	    Object* new_obj = enemy_creator.CreateEnemy(player->Position(), objects);
		if (new_obj != nullptr) {
			objects.push_back(new_obj);
		}

		glm::mat4 Projection = glm::perspective(glm::radians(player->FOV()), GLfloat(w / h), player->Box(), 300.0f);
		glm::mat4 View = glm::lookAt(
			player->Position(),
			player->Position() + player->CameraDirection(),
			player->CameraUp()
		);

		glUseProgram(simpleProgramID);

		glUniformMatrix4fv(SimpleProjectionID, 1, GL_FALSE, &Projection[0][0]);
		glUniformMatrix4fv(SimpleViewID, 1, GL_FALSE, &View[0][0]);

		GLfloat move = 0.0f;
		glUniform1fv(SimpleMoveID, 1, &move);

		glm::mat4 Scale = glm::scale(glm::mat4(), glm::vec3(skybox->Box(), skybox->Box(), skybox->Box()));
		glm::mat4 Translate = glm::translate(glm::mat4(), skybox->Position());

		glm::mat4 Model = Translate * Scale;
		glUniformMatrix4fv(SimpleModelID, 1, GL_FALSE, &Model[0][0]);

		skybox->Draw(SimpleTextureID, vertexbuffer, uvbuffer, normalbuffer);

		for (Object* obj : objects) {
			if (dynamic_cast<Projectile*>(obj) != nullptr) {
				move = dynamic_cast<Projectile*>(obj)->ExplodedMove();
				glUniform1fv(SimpleMoveID, 1, &move);

				glm::mat4 Scale = glm::scale(glm::mat4(), glm::vec3(obj->Box(), obj->Box(), obj->Box()) / 1.5f);
				glm::mat4 Translate = glm::translate(glm::mat4(), obj->Position());

				glm::vec3 direction = obj->GetDirection();
				glm::vec3 base_direction(1.0f, 0.0f, 0.0f);
				glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
				GLfloat sin_value = glm::dot(glm::cross(base_direction, direction), up);
				GLfloat cos_value = glm::dot(base_direction, direction);

				glm::mat4 Rotate = glm::rotate(glm::mat4(), std::atan2(sin_value, cos_value), up);

				glm::mat4 Model = Translate * Rotate * Scale;
				glUniformMatrix4fv(SimpleModelID, 1, GL_FALSE, &Model[0][0]);

				obj->Draw(SimpleTextureID, vertexbuffer, uvbuffer, normalbuffer);
			}
		}

		glUseProgram(programID);

		glUniformMatrix4fv(ProjectionID, 1, GL_FALSE, &Projection[0][0]);
		glUniformMatrix4fv(ViewID, 1, GL_FALSE, &View[0][0]);

		std::vector<Object*> projectiles;

		for (Object* obj : objects) {
			Projectile* proj = dynamic_cast<Projectile*>(obj);
			if ((proj != nullptr) && !proj->Exploded()) {
				projectiles.push_back(proj);
			}
		}

		std::vector<glm::vec3> pos;
		std::vector<glm::vec3> light_colors;
		std::vector<float> powers;

		for (Object* proj : projectiles) {
			pos.push_back(proj->Position());
			light_colors.push_back(glm::vec3(1.0f, 0.3f, 0.3f));
			powers.push_back(proj->Box() * 1000.0f);
		}

		int num = std::min(pos.size(), size_t(ShaderNum));

		if (num > 0) {
			glUniform1iv(NumID, 1, &num);
			glUniform3fv(LightID, num, &pos[0][0]);
			glUniform3fv(LightColorID, num, &light_colors[0][0]);
			glUniform1fv(LightPowerID, num, &powers[0]);
		} else {
			glUniform1iv(NumID, 1, &num);
		}

		glm::vec3 light = glm::vec3(0.5f, 0.5f, 0.5f);
		glUniform3fv(SpecularID, 1, &light[0]);
		light = glm::vec3(0.3f, 0.3f, 0.3f);
		glUniform3fv(AmbientID, 1, &light[0]);

		for (Object* obj : objects) {
			if (dynamic_cast<Projectile*>(obj) == nullptr) {
				glm::mat4 Scale = glm::scale(glm::mat4(), glm::vec3(obj->Box(), obj->Box(), obj->Box()) / 1.5f);
				glm::mat4 Translate = glm::translate(glm::mat4(), obj->Position());

				glm::vec3 direction = obj->GetDirection();
				glm::vec3 base_direction(1.0f, 0.0f, 0.0f);
				glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
				GLfloat sin_value = glm::dot(glm::cross(base_direction, direction), up);
				GLfloat cos_value = glm::dot(base_direction, direction);

				glm::mat4 Rotate = glm::rotate(glm::mat4(), std::atan2(sin_value, cos_value), up);

				glm::mat4 Model = Translate * Rotate * Scale;
				glUniformMatrix4fv(ModelID, 1, GL_FALSE, &Model[0][0]);

				obj->Draw(TextureID, vertexbuffer, uvbuffer, normalbuffer);
			}
		}

		size_t current_enemies = 0;

		for (Object* obj : objects) {
			if (dynamic_cast<Dummy*>(obj) != nullptr || dynamic_cast<Enemy*>(obj) != nullptr) {
				++current_enemies;
			}
		}

		printText2D(std::string("HP: " + std::to_string(player->HP())).data(), 10, 550, 20);
		printText2D(std::string("Killed: " + std::to_string(player->Killed())).data(), 10, 530, 20);
		printText2D(std::string("Current enemies: " + std::to_string(current_enemies)).data(), 10, 510, 20);

		glfwSwapBuffers(window);
		glfwPollEvents();

	}
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);

	for (Object* obj : objects) {
		delete obj;
	}

	delete skybox;

	cleanupText2D();
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteProgram(programID);
	glDeleteProgram(simpleProgramID);
	glDeleteVertexArrays(1, &VertexArrayID);

	glfwTerminate();

	return 0;
}

