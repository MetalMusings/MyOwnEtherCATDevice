var formMock = {};

function buildMockFormHelper(formValues = null) {
    const defaultFormValues = getFormDefaultValues().form;
    
    if (!formValues) {
        formValues = defaultFormValues;
    }
    
    formMock = { }

    Object.keys(defaultFormValues).forEach(formControlName => {
        const formControl = { 
            name: formControlName, 
        };
        setFormControlValue(formControl, formValues[formControlName]);
        formMock[formControlName] = formControl;
    });

    return formMock;
}

function getForm() {
    return formMock;
}