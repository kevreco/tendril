#include "app.hpp"

#include "../tests/tests.hpp"

int main(void)
{
	printf("Starting tests\n");

	td::tests::all();

	printf("Starting app\n");
	{
		td_app app;

		char title[256] = { 0 };
		snprintf(title, 256, "%s %s (%d)", TD_APP_NAME, TD_APP_VERSION_TEXT, TD_APP_VERSION_NUMBER);

		app.set_name(TD_APP_NAME);
		app.set_title(title);
		app.set_icon_path("./tendril.ico");
		app.set_initial_size(1080, 720);
		app.show();
	}

	return 0;
}
