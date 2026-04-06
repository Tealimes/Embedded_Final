################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: MSP430 Compiler'
	"C:/ti/ccs1260/ccs/tools/compiler/ti-cgt-msp430_4.4.8/bin/cl430" -vmspx --abi=eabi --data_model=restricted --use_hw_mpy=F5 --include_path="C:/ti/ccs1260/ccs/ccs_base/msp430/include" --include_path="C:/Users/alexp/workspace_v12/FINAL_REAL" --include_path="C:/ti/ccs1260/ccs/tools/compiler/ti-cgt-msp430_4.4.8/include" --advice:power=all --advice:hw_config=all -g --define=__MSP430FR6989__ --define=_MPU_ENABLE --display_error_number --diag_warning=225 --diag_wrap=off --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU40 --printf_support=minimal --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


