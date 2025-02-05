// SPDX-License-Identifier: GPL-2.0-only
/*
 * Layerscape GPIO CPLD driver
 *
 * Copyright 2023 NXP
 */

#include <linux/device.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/regmap.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

enum qixis_cpld_gpio_type {
	LX2160ARDB_CPLD_GPIO = 0,
	LS1046AQDS_STAT_PRES2_CPLD_GPIO,
};

struct qixis_cpld_gpio_config {
	enum qixis_cpld_gpio_type type;
	unsigned int input_lines;
};

static struct qixis_cpld_gpio_config lx2160ardb_cpld_gpio_cfg = {
	.type = LX2160ARDB_CPLD_GPIO,
	.input_lines = GENMASK(7, 1),
};

static struct qixis_cpld_gpio_config ls1046aqds_stat_pres2_cpld_gpio_cfg = {
	.type = LS1046AQDS_STAT_PRES2_CPLD_GPIO,
	.input_lines = GENMASK(7, 0),
};

static int qixis_cpld_gpio_get_direction(struct gpio_regmap *gpio, unsigned int offset)
{
	struct qixis_cpld_gpio_config *cfg = gpio_regmap_get_drvdata(gpio);

	if (cfg->input_lines & BIT(offset))
		return GPIO_LINE_DIRECTION_IN;
	else
		return GPIO_LINE_DIRECTION_OUT;
}

static const struct regmap_config regmap_config_8r_8v = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int qixis_cpld_gpio_probe(struct platform_device *pdev)
{
	const struct qixis_cpld_gpio_config *cfg;
	struct gpio_regmap_config config = {0};
	struct regmap *regmap;
	void __iomem *reg;
	u32 base;
	int ret;

	if (!pdev->dev.parent)
		return -ENODEV;

	cfg = device_get_match_data(&pdev->dev);
	if (!cfg)
		return -ENODEV;

	ret = device_property_read_u32(&pdev->dev, "reg", &base);
	if (ret)
		return ret;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		/* In case there is no regmap configured by the parent device,
		 * create our own.
		 */
		reg = devm_platform_ioremap_resource(pdev, 0);
		if (!reg)
			return -ENODEV;

		regmap = devm_regmap_init_mmio(&pdev->dev, reg, &regmap_config_8r_8v);
		if (!regmap)
			return -ENODEV;

		/* In this case, the offset of our register is 0 inside the
		 * regmap area that we just created.
		 */
		base = 0;
	}

	config.get_direction = qixis_cpld_gpio_get_direction;
	config.drvdata = (void *)cfg;
	config.regmap = regmap;
	config.parent = &pdev->dev;
	config.ngpio_per_reg = 8;
	config.ngpio = 8;

	switch (cfg->type) {
	case LX2160ARDB_CPLD_GPIO:
	case LS1046AQDS_STAT_PRES2_CPLD_GPIO:
		config.reg_dat_base = GPIO_REGMAP_ADDR(base);
		config.reg_set_base = GPIO_REGMAP_ADDR(base);
		break;
	}

	return PTR_ERR_OR_ZERO(devm_gpio_regmap_register(&pdev->dev, &config));
}

static const struct of_device_id qixis_cpld_gpio_of_match[] = {
	{
		.compatible = "fsl,lx2160a-rdb-qixis-cpld-gpio",
		.data = &lx2160ardb_cpld_gpio_cfg
	},
	{
		.compatible = "fsl,ls1046a-qds-qixis-stat-pres2-cpld-gpio",
		.data = &ls1046aqds_stat_pres2_cpld_gpio_cfg
	},

	{}
};
MODULE_DEVICE_TABLE(of, qixis_cpld_gpio_of_match);

static struct platform_driver qixis_cpld_gpio_driver = {
	.probe = qixis_cpld_gpio_probe,
	.driver = {
		.name = "gpio-qixis-cpld",
		.of_match_table = qixis_cpld_gpio_of_match,
	},
};
module_platform_driver(qixis_cpld_gpio_driver);
